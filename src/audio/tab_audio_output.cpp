#include "tab_audio_output.h"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace opengod {
namespace {
constexpr std::size_t kMaxPendingPackets = 12;
constexpr std::size_t kMaxDevicePackets = 3;

std::string failure(const char* operation, HRESULT result) {
  char code[16]{};
  std::snprintf(code, sizeof(code), "0x%08lX", static_cast<unsigned long>(result));
  return std::string(operation) + " failed (" + code + ")";
}
}  // namespace

struct TabAudioOutput::Impl final : IXAudio2VoiceCallback, IXAudio2EngineCallback {
  struct Packet {
    std::vector<float> samples;
    int frames = 0;
    std::atomic<bool> complete{false};
  };

  // All device operations and destruction occur on the COM-initialized worker.
  struct Device {
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* source = nullptr;
    Impl* callbacks = nullptr;
    bool registered = false;
    std::deque<std::unique_ptr<Packet>> submitted;

    ~Device() {
      // DestroyVoice waits for callbacks and releases all buffer references.
      if (source)
        source->DestroyVoice();
      submitted.clear();
      if (master)
        master->DestroyVoice();
      if (registered)
        engine->UnregisterForCallbacks(callbacks);
      if (engine)
        engine->Release();
    }
  };

  mutable std::mutex mutex;
  std::condition_variable changed;
  std::deque<std::unique_ptr<Packet>> pending;
  std::size_t pending_frames = 0;
  std::uint64_t generation = 0;
  bool enabled = false;
  bool quitting = false;
  int rate = 48000;
  int channels = 2;
  float gain = 1.0f;
  std::string last_error;
  std::atomic<bool> device_ready{false};
  std::atomic<HRESULT> callback_error{S_OK};
  std::atomic<std::uint64_t> dropped{0};
  std::atomic<std::uint64_t> completed{0};
  std::thread worker;

  Impl() : worker([this] { run(); }) {}
  ~Impl() {
    {
      std::lock_guard lock(mutex);
      quitting = true;
      enabled = false;
      ++generation;
      pending.clear();
      pending_frames = 0;
    }
    changed.notify_one();
    worker.join();
  }

  void report(std::uint64_t current, std::string message) {
    std::lock_guard lock(mutex);
    if (current != generation)
      return;
    last_error = std::move(message);
    enabled = false;
    device_ready.store(false);
    pending.clear();
    pending_frames = 0;
  }

  HRESULT open(Device& device, int sample_rate, int channel_count, float level) {
    device.callbacks = this;
    HRESULT result = XAudio2Create(&device.engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result))
      return result;
    result = device.engine->RegisterForCallbacks(this);
    if (FAILED(result))
      return result;
    device.registered = true;
    result = device.engine->CreateMasteringVoice(&device.master);
    if (FAILED(result))
      return result;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = static_cast<WORD>(channel_count);
    format.nSamplesPerSec = static_cast<DWORD>(sample_rate);
    format.wBitsPerSample = 32;
    format.nBlockAlign = static_cast<WORD>(channel_count * sizeof(float));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    result = device.engine->CreateSourceVoice(&device.source, &format, 0,
                                              XAUDIO2_DEFAULT_FREQ_RATIO, this);
    if (FAILED(result))
      return result;
    result = device.source->SetVolume(level);
    if (FAILED(result))
      return result;
    return device.source->Start();
  }

  void stream(std::uint64_t current, int sample_rate, int channel_count, float level) {
    Device device;
    callback_error.store(S_OK);
    HRESULT result = open(device, sample_rate, channel_count, level);
    if (FAILED(result)) {
      report(current, failure("Opening Windows audio output", result));
      return;
    }
    {
      std::lock_guard lock(mutex);
      if (quitting || !enabled || generation != current)
        return;
      device_ready.store(true);
    }
    for (;;) {
      while (!device.submitted.empty() && device.submitted.front()->complete.load()) {
        device.submitted.pop_front();
      }
      result = callback_error.load();
      if (FAILED(result)) {
        report(current, failure("Windows audio device", result));
        return;
      }
      std::unique_ptr<Packet> packet;
      float requested_gain = level;
      {
        std::unique_lock lock(mutex);
        if (quitting || !enabled || generation != current)
          return;
        requested_gain = gain;
        if (!pending.empty() && device.submitted.size() < kMaxDevicePackets) {
          packet = std::move(pending.front());
          pending.pop_front();
          pending_frames -= static_cast<std::size_t>(packet->frames);
        } else if (requested_gain == level) {
          changed.wait_for(lock, std::chrono::milliseconds(5));
          continue;
        }
      }
      if (requested_gain != level) {
        result = device.source->SetVolume(requested_gain);
        if (FAILED(result)) {
          report(current, failure("Setting tab gain", result));
          return;
        }
        level = requested_gain;
      }
      if (packet) {
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(packet->samples.size() * sizeof(float));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(packet->samples.data());
        buffer.pContext = packet.get();
        // Own the storage before submission: completion may arrive immediately.
        device.submitted.push_back(std::move(packet));
        result = device.source->SubmitSourceBuffer(&buffer);
        if (FAILED(result)) {
          report(current, failure("Submitting tab PCM", result));
          return;
        }
      }
    }
  }

  void run() {
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::uint64_t observed = 0;
    for (;;) {
      int sample_rate = 0;
      int channel_count = 0;
      float level = 0;
      {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return quitting || generation != observed; });
        if (quitting)
          break;
        observed = generation;
        device_ready.store(false);
        if (!enabled)
          continue;
        sample_rate = rate;
        channel_count = channels;
        level = gain;
      }
      if (FAILED(com_result)) {
        report(observed, failure("Initializing audio worker COM", com_result));
      } else {
        stream(observed, sample_rate, channel_count, level);
      }
      device_ready.store(false);
    }
    if (SUCCEEDED(com_result))
      CoUninitialize();
  }

  void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
  void STDMETHODCALLTYPE OnStreamEnd() override {}
  void STDMETHODCALLTYPE OnBufferStart(void*) override {}
  void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
  void STDMETHODCALLTYPE OnBufferEnd(void* context) override {
    auto* packet = static_cast<Packet*>(context);
    completed.fetch_add(static_cast<std::uint64_t>(packet->frames));
    packet->complete.store(true);
    changed.notify_one();
  }
  void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT result) override {
    callback_error.store(result);
    changed.notify_one();
  }
  void STDMETHODCALLTYPE OnProcessingPassStart() override {}
  void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
  void STDMETHODCALLTYPE OnCriticalError(HRESULT result) override {
    callback_error.store(result);
    changed.notify_one();
  }
};

TabAudioOutput::TabAudioOutput() : impl_(std::make_unique<Impl>()) {}
TabAudioOutput::~TabAudioOutput() = default;

void TabAudioOutput::start(int sample_rate, int channel_count) {
  {
    std::lock_guard lock(impl_->mutex);
    ++impl_->generation;
    impl_->pending.clear();
    impl_->pending_frames = 0;
    impl_->device_ready.store(false);
    impl_->enabled =
        sample_rate >= 8000 && sample_rate <= 192000 && (channel_count == 1 || channel_count == 2);
    impl_->last_error =
        impl_->enabled ? "" : "Unsupported audio format: mono/stereo, 8-192 kHz required";
    impl_->rate = sample_rate;
    impl_->channels = channel_count;
  }
  impl_->changed.notify_one();
}

void TabAudioOutput::push(const float** data, int frames) {
  std::lock_guard lock(impl_->mutex);
  if (!impl_->enabled || impl_->quitting)
    return;
  if (!data || frames <= 0 || frames > impl_->rate / 20) {
    impl_->last_error = "Invalid PCM packet: maximum duration is 50 ms";
    impl_->dropped.fetch_add(1);
    return;
  }
  for (int channel = 0; channel < impl_->channels; ++channel) {
    if (!data[channel]) {
      impl_->last_error = "Invalid PCM channel pointer";
      impl_->dropped.fetch_add(1);
      return;
    }
  }
  auto packet = std::make_unique<Impl::Packet>();
  packet->frames = frames;
  packet->samples.resize(static_cast<std::size_t>(frames) * impl_->channels);
  for (int frame = 0; frame < frames; ++frame) {
    for (int channel = 0; channel < impl_->channels; ++channel) {
      const float value = data[channel][frame];
      packet->samples[static_cast<std::size_t>(frame) * impl_->channels + channel] =
          std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    }
  }
  // Prefer current audio over accumulating latency if the device stalls.
  const auto frame_budget = static_cast<std::size_t>(impl_->rate / 10);
  while (!impl_->pending.empty() &&
         (impl_->pending.size() >= kMaxPendingPackets ||
          impl_->pending_frames + static_cast<std::size_t>(frames) > frame_budget)) {
    impl_->pending_frames -= static_cast<std::size_t>(impl_->pending.front()->frames);
    impl_->pending.pop_front();
    impl_->dropped.fetch_add(1);
  }
  impl_->pending_frames += static_cast<std::size_t>(frames);
  impl_->pending.push_back(std::move(packet));
  impl_->changed.notify_one();
}

void TabAudioOutput::stop() {
  {
    std::lock_guard lock(impl_->mutex);
    impl_->enabled = false;
    ++impl_->generation;
    impl_->pending.clear();
    impl_->pending_frames = 0;
    impl_->device_ready.store(false);
  }
  impl_->changed.notify_one();
}

void TabAudioOutput::set_gain(float level) {
  {
    std::lock_guard lock(impl_->mutex);
    if (!std::isfinite(level) || level < 0.0f || level > 1.0f) {
      impl_->last_error = "Tab gain must be finite and within 0..1";
      return;
    }
    impl_->gain = level;
  }
  impl_->changed.notify_one();
}

std::string TabAudioOutput::error() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->last_error;
}
bool TabAudioOutput::ready() const {
  return impl_->device_ready.load();
}
std::uint64_t TabAudioOutput::dropped_packets() const {
  return impl_->dropped.load();
}
std::uint64_t TabAudioOutput::completed_frames() const {
  return impl_->completed.load();
}

}  // namespace opengod
