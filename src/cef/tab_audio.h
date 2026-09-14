#pragma once
#include <mutex>
#include <string>
#include "audio/tab_audio_output.h"
#include "include/cef_audio_handler.h"
namespace opengod {
// A callback object owns the stream, never a raw Host/Tab pointer. CEF may retain
// it after the logical tab closes; closed_ makes all late callbacks inert.
class TabAudio final : public CefAudioHandler {
 public:
  void set_settings(bool muted, int volume);
  void close();
  std::string error() const;
  bool GetAudioParameters(CefRefPtr<CefBrowser>, CefAudioParameters& params) override;
  void OnAudioStreamStarted(CefRefPtr<CefBrowser>,
                            const CefAudioParameters&,
                            int channels) override;
  void OnAudioStreamPacket(CefRefPtr<CefBrowser>, const float** data, int frames, int64_t) override;
  void OnAudioStreamStopped(CefRefPtr<CefBrowser>) override;
  void OnAudioStreamError(CefRefPtr<CefBrowser>, const CefString&) override;

 private:
  mutable std::mutex mutex_;
  bool closed_ = false;
  std::string capture_error_;
  TabAudioOutput output_;
  IMPLEMENT_REFCOUNTING(TabAudio);
};
}  // namespace opengod
