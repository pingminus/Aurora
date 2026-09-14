#include "tab_audio.h"
namespace opengod {
void TabAudio::set_settings(bool muted, int volume) {
  std::lock_guard lock(mutex_);
  if (!closed_)
    output_.set_gain(muted ? 0.0f : static_cast<float>(volume) / 100.0f);
}
void TabAudio::close() {
  std::lock_guard lock(mutex_);
  closed_ = true;
  output_.stop();
}
std::string TabAudio::error() const {
  std::lock_guard lock(mutex_);
  return capture_error_.empty() ? output_.error() : capture_error_;
}
bool TabAudio::GetAudioParameters(CefRefPtr<CefBrowser>, CefAudioParameters& params) {
  std::lock_guard lock(mutex_);
  params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
  params.sample_rate = 48000;
  params.frames_per_buffer = 480;
  return !closed_;
}
void TabAudio::OnAudioStreamStarted(CefRefPtr<CefBrowser>,
                                    const CefAudioParameters& params,
                                    int channels) {
  std::lock_guard lock(mutex_);
  if (closed_)
    return;
  capture_error_.clear();
  output_.start(params.sample_rate, channels);
}
void TabAudio::OnAudioStreamPacket(CefRefPtr<CefBrowser>, const float** data, int frames, int64_t) {
  std::lock_guard lock(mutex_);
  if (!closed_)
    output_.push(data, frames);
}
void TabAudio::OnAudioStreamStopped(CefRefPtr<CefBrowser>) {
  std::lock_guard lock(mutex_);
  output_.stop();
}
void TabAudio::OnAudioStreamError(CefRefPtr<CefBrowser>, const CefString&) {
  std::lock_guard lock(mutex_);
  if (closed_)
    return;
  capture_error_ = "Tab audio capture failed. Reload to retry.";
  output_.stop();
}
}  // namespace opengod
