// Process-scoped diagnostic capture. No microphone or desktop fallback exists.
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using Microsoft::WRL::ComPtr;
constexpr unsigned kRate = 48000;
constexpr unsigned kChannels = 2;

class Handle {
 public:
  explicit Handle(HANDLE value) : value_(value) {}
  ~Handle() {
    if (value_ && value_ != INVALID_HANDLE_VALUE)
      CloseHandle(value_);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE get() const { return value_; }

 private:
  HANDLE value_;
};

std::string hex_error(HRESULT result) {
  char text[16]{};
  std::snprintf(text, sizeof(text), "0x%08lX", static_cast<unsigned long>(result));
  return text;
}
void check(HRESULT result, const char* operation) {
  if (FAILED(result))
    throw std::runtime_error(std::string(operation) + ": " + hex_error(result));
}
std::string json_string(std::string_view text) {
  std::string out = "\"";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      char escaped[7]{};
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
      out += escaped;
    } else
      out += static_cast<char>(c);
  }
  return out + '"';
}

// FtmBase makes the callback agile; every COM caller uses an MTA. Parameters and
// completion event live with this ref-counted object, including a timed-out call.
class Activation final : public Microsoft::WRL::RuntimeClass<
                             Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                             Microsoft::WRL::FtmBase,
                             IActivateAudioInterfaceCompletionHandler> {
 public:
  Activation() : done(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}
  HRESULT STDMETHODCALLTYPE
  ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
    ComPtr<IUnknown> object;
    HRESULT activated = E_UNEXPECTED;
    result = operation->GetActivateResult(&activated, &object);
    if (SUCCEEDED(result))
      result = activated;
    if (SUCCEEDED(result))
      result = object.As(&client);
    SetEvent(done.get());
    return S_OK;
  }
  Handle done;
  AUDIOCLIENT_ACTIVATION_PARAMS parameters{};
  PROPVARIANT property{};
  HRESULT result = E_PENDING;
  ComPtr<IAudioClient> client;
};

struct Measurements {
  double rms = 0;
  double peak = 0;
  std::array<double, 2> tone440{};
  std::array<double, 2> tone660{};
};
Measurements measure(const std::vector<float>& pcm) {
  Measurements out;
  const std::size_t frames = pcm.size() / kChannels;
  if (frames < 2)
    return out;
  double squares = 0;
  double weights = 0;
  std::array<std::array<double, 2>, 2> cosine{}, sine{};
  constexpr std::array<double, 2> frequencies{440, 660};
  for (std::size_t i = 0; i < frames; ++i) {
    const double window = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * static_cast<double>(i) /
                                               static_cast<double>(frames - 1));
    weights += window;
    for (unsigned ch = 0; ch < kChannels; ++ch) {
      const double value = pcm[i * kChannels + ch];
      squares += value * value;
      out.peak = std::max(out.peak, std::abs(value));
      for (unsigned tone = 0; tone < frequencies.size(); ++tone) {
        const double phase =
            2 * std::numbers::pi * frequencies[tone] * static_cast<double>(i) / kRate;
        cosine[tone][ch] += value * window * std::cos(phase);
        sine[tone][ch] += value * window * std::sin(phase);
      }
    }
  }
  out.rms = std::sqrt(squares / static_cast<double>(pcm.size()));
  for (unsigned ch = 0; ch < kChannels; ++ch) {
    out.tone440[ch] = 2 * std::hypot(cosine[0][ch], sine[0][ch]) / weights;
    out.tone660[ch] = 2 * std::hypot(cosine[1][ch], sine[1][ch]) / weights;
  }
  return out;
}

double channel_magnitude(const std::array<double, 2>& channels) {
  return std::hypot(channels[0], channels[1]) / std::sqrt(2.0);
}

int self_test() {
  std::vector<float> pcm(kRate * 2 * kChannels);
  for (std::size_t i = 0; i < pcm.size() / kChannels; ++i) {
    const double phase = 2 * std::numbers::pi * static_cast<double>(i) / kRate;
    const float value =
        static_cast<float>(0.25 * std::sin(440 * phase) + 0.1 * std::sin(660 * phase));
    pcm[i * 2] = value;
    pcm[i * 2 + 1] = -value;  // Opposite stereo phase must not cancel the result.
  }
  const auto full = measure(pcm);
  for (auto& sample : pcm)
    sample *= 0.25f;
  const auto quarter = measure(pcm);
  std::fill(pcm.begin(), pcm.end(), 0.0f);
  const auto silence = measure(pcm);
  const bool good = std::abs(channel_magnitude(full.tone440) - 0.25) < 0.00001 &&
                    std::abs(channel_magnitude(full.tone660) - 0.1) < 0.00001 &&
                    std::abs(channel_magnitude(quarter.tone440) / channel_magnitude(full.tone440) -
                             0.25) < 0.00001 &&
                    silence.rms == 0 && channel_magnitude(silence.tone440) == 0;
  std::cout << "{\"status\":\"" << (good ? "passed" : "failed")
            << "\",\"test\":\"synthetic_spectrum_only_no_capture\"}\n";
  return good ? 0 : 1;
}

void capture(DWORD pid, double seconds) {
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
  if (!process.get())
    throw std::runtime_error("Cannot open target process: " +
                             hex_error(HRESULT_FROM_WIN32(GetLastError())));
  if (WaitForSingleObject(process.get(), 0) != WAIT_TIMEOUT)
    throw std::runtime_error("Target process is not running");
  const auto activation = Microsoft::WRL::Make<Activation>();
  if (!activation || !activation->done.get())
    throw std::runtime_error("Cannot allocate activation completion event");
  activation->parameters.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  activation->parameters.ProcessLoopbackParams.TargetProcessId = pid;
  activation->parameters.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  activation->property.vt = VT_BLOB;
  activation->property.blob.cbSize = sizeof(activation->parameters);
  activation->property.blob.pBlobData = reinterpret_cast<BYTE*>(&activation->parameters);
  ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
  check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                    &activation->property, activation.Get(), &operation),
        "Activate process loopback");
  if (WaitForSingleObject(activation->done.get(), 10000) != WAIT_OBJECT_0)
    throw std::runtime_error("Process-loopback activation timed out or wait failed (10 seconds)");
  check(activation->result, "Complete process-loopback activation");
  auto client = activation->client;
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = kChannels;
  format.nSamplesPerSec = kRate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = kChannels * sizeof(std::int16_t);
  format.nAvgBytesPerSec = kRate * format.nBlockAlign;
  check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                           AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                               AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                               AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                           0, 0, &format, nullptr),
        "Initialize stereo PCM16 capture");
  Handle samples_ready(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  if (!samples_ready.get())
    throw std::runtime_error("Cannot create capture event");
  check(client->SetEventHandle(samples_ready.get()), "Set capture event");
  ComPtr<IAudioCaptureClient> reader;
  check(client->GetService(IID_PPV_ARGS(&reader)), "Get capture client");
  const auto target_frames = static_cast<std::size_t>(seconds * kRate);
  std::vector<float> pcm;
  pcm.reserve(target_frames * kChannels);
  unsigned packets = 0, discontinuities = 0, timestamp_errors = 0, silent_packets = 0;
  check(client->Start(), "Start process capture");
  struct Stopper {
    IAudioClient* client;
    ~Stopper() { client->Stop(); }
  } stop{client.Get()};
  const auto started = std::chrono::steady_clock::now();
  // Allow one second for initial data; never wait indefinitely for a silent or
  // unsupported target. Missing packets are reported, not fabricated as silence.
  const auto deadline = started + std::chrono::duration<double>(seconds + 1.0);
  while (pcm.size() / kChannels < target_frames && std::chrono::steady_clock::now() < deadline) {
    if (WaitForSingleObject(process.get(), 0) != WAIT_TIMEOUT)
      throw std::runtime_error("Target process exited during capture");
    const DWORD wait = WaitForSingleObject(samples_ready.get(), 50);
    if (wait == WAIT_FAILED)
      throw std::runtime_error("Capture event wait failed");
    UINT32 available = 0;
    check(reader->GetNextPacketSize(&available), "Read packet size");
    while (available && pcm.size() / kChannels < target_frames) {
      BYTE* bytes = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      check(reader->GetBuffer(&bytes, &frames, &flags, nullptr, nullptr), "Read process PCM");
      const auto keep = std::min<std::size_t>(frames, target_frames - pcm.size() / kChannels);
      const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
      if (silent)
        ++silent_packets;
      if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        ++discontinuities;
      if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
        ++timestamp_errors;
      if (!silent && !bytes) {
        reader->ReleaseBuffer(frames);
        throw std::runtime_error("Capture returned a null non-silent packet");
      }
      const auto* values = reinterpret_cast<const std::int16_t*>(bytes);
      for (std::size_t i = 0; i < keep * kChannels; ++i)
        pcm.push_back(silent ? 0.0f : values[i] / 32768.0f);
      check(reader->ReleaseBuffer(frames), "Release PCM buffer");
      ++packets;
      check(reader->GetNextPacketSize(&available), "Read next packet size");
    }
  }
  check(client->Stop(), "Stop process capture");
  if (pcm.empty())
    throw std::runtime_error(
        "No process PCM packets received; no silence or gain conclusion is possible");
  const auto m = measure(pcm);
  const auto frames = pcm.size() / kChannels;
  std::cout << std::setprecision(10) << "{\"status\":\""
            << (frames == target_frames ? "captured" : "partial")
            << "\",\"scope\":\"include_target_process_tree\",\"pid\":" << pid
            << ",\"sample_rate\":" << kRate << ",\"channels\":2,\"format\":\"PCM16\""
            << ",\"requested_seconds\":" << seconds
            << ",\"captured_seconds\":" << static_cast<double>(frames) / kRate
            << ",\"frames\":" << frames << ",\"packets\":" << packets
            << ",\"silent_packets\":" << silent_packets
            << ",\"discontinuities\":" << discontinuities
            << ",\"timestamp_errors\":" << timestamp_errors << ",\"rms\":" << m.rms
            << ",\"peak\":" << m.peak << ",\"amplitude_440\":" << channel_magnitude(m.tone440)
            << ",\"amplitude_660\":" << channel_magnitude(m.tone660)
            << ",\"amplitude_440_channels\":[" << m.tone440[0] << ',' << m.tone440[1]
            << "],\"amplitude_660_channels\":[" << m.tone660[0] << ',' << m.tone660[1] << "]}\n";
}

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--self-test")
    return self_test();
  HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  struct Uninit {
    HRESULT result;
    ~Uninit() {
      if (SUCCEEDED(result))
        CoUninitialize();
    }
  } cleanup{com};
  try {
    DWORD pid = 0;
    double seconds = 2;
    bool has_pid = false, has_seconds = false;
    for (int i = 1; i < argc; ++i) {
      const std::string_view key(argv[i]);
      if (++i >= argc)
        throw std::runtime_error("Expected --pid N [--seconds 0.25..10]");
      const std::string_view value(argv[i]);
      if (key == "--pid" && !has_pid) {
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), pid);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !pid)
          throw std::runtime_error("Invalid target PID");
        has_pid = true;
      } else if (key == "--seconds" && !has_seconds) {
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
            !std::isfinite(seconds) || seconds < 0.25 || seconds > 10)
          throw std::runtime_error("Capture seconds must be within 0.25..10");
        has_seconds = true;
      } else
        throw std::runtime_error("Unknown or repeated argument");
    }
    if (!has_pid)
      throw std::runtime_error("Expected --pid N [--seconds 0.25..10]");
    check(com, "Initialize COM");
    capture(pid, seconds);
    return 0;
  } catch (const std::exception& error) {
    std::cout << "{\"status\":\"error\",\"error\":" << json_string(error.what()) << "}\n";
    return 1;
  }
}
