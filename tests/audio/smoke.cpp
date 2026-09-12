#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
#include "../../src/audio/tab_audio_output.h"
using namespace std::chrono_literals;
int main() {
  aurora::TabAudioOutput out;
  out.start(48000, 8);
  if (out.error().empty() || out.ready())
    return 1;
  out.set_gain(std::numeric_limits<float>::quiet_NaN());
  if (out.error().empty())
    return 2;
  out.set_gain(0);
  out.start(48000, 2);
  for (int i = 0; i < 500 && !out.ready() && out.error().empty(); ++i)
    std::this_thread::sleep_for(10ms);
  if (!out.ready()) {
    std::cerr << "Device unavailable: " << out.error() << '\n';
    return 3;
  }
  std::vector<float> left(480, 0.0f), right(480, 0.0f);
  const float* planes[]{left.data(), right.data()};
  for (int i = 0; i < 20; ++i) {
    out.push(planes, 480);
    std::this_thread::sleep_for(10ms);
  }
  for (int i = 0; i < 100 && out.completed_frames() < 4800; ++i)
    std::this_thread::sleep_for(10ms);
  if (out.completed_frames() < 4800) {
    std::cerr << out.error();
    return 4;
  }
  for (int i = 0; i < 10000; ++i)
    out.push(planes, 480);
  if (!out.dropped_packets())
    return 5;
  out.stop();
  if (out.ready())
    return 6;
  for (int i = 0; i < 100; ++i) {
    out.start(48000, 2);
    out.push(planes, 480);
    out.set_gain(0.25f);
    out.stop();
  }
  out.start(48000, 1);
  for (int i = 0; i < 500 && !out.ready() && out.error().empty(); ++i)
    std::this_thread::sleep_for(10ms);
  if (!out.ready() || !out.error().empty())
    return 7;
  out.stop();
  std::cout << "PASS invalid formats/gain, real silent device playback, overflow, 100 restarts, "
               "mono restart; completed_frames="
            << out.completed_frames() << ", dropped=" << out.dropped_packets() << '\n';
}
