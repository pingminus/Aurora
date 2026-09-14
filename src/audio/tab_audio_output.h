#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace opengod {

// One captured CEF browser stream, played through the default Windows device.
// Calls are thread-safe; start/stop are asynchronous. The owner must prevent
// further calls before destruction, which joins the device worker.
class TabAudioOutput {
 public:
  TabAudioOutput();
  ~TabAudioOutput();
  TabAudioOutput(const TabAudioOutput&) = delete;
  TabAudioOutput& operator=(const TabAudioOutput&) = delete;

  // Mono/stereo float PCM, 8-192 kHz. Restarts discard queued old-stream audio.
  void start(int sample_rate, int channels);
  // Copies planar PCM before returning; caller retains ownership of data.
  void push(const float** data, int frames);
  void stop();
  // Linear amplitude, 0..1. Invalid values are rejected and reported by error().
  void set_gain(float gain);
  [[nodiscard]] std::string error() const;
  [[nodiscard]] bool ready() const;
  [[nodiscard]] std::uint64_t dropped_packets() const;
  // Device buffer-completion telemetry, not proof of audible speaker output.
  [[nodiscard]] std::uint64_t completed_frames() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace opengod
