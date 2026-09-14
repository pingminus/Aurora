#pragma once
#include "core/cve_feed.h"
#include "include/cef_urlrequest.h"
#include <mutex>
namespace opengod {
// Request/JSON work is confined to CEF's background thread. UI reads a published snapshot.
class CveService final : public CefBaseRefCounted {
 public:
  void poll(bool refresh);
  std::string snapshot();
  void shutdown();
  void complete(uint64_t token, CefRefPtr<CefURLRequest> request);
  void receive(uint64_t token, CefRefPtr<CefURLRequest> request, const void* data, size_t size);
 private:
  void begin(bool refresh);
  void fetch();
  void publish();
  void fail(const std::string& error);
  cve::Cache cache_;
  std::optional<cve::Batch> batch_;
  CefRefPtr<CefURLRequest> request_;
  std::string body_;
  uint64_t generation_ = 0;
  bool stopped_ = false;
  std::mutex mutex_;
  std::string snapshot_ = R"({"version":1,"status":"loading","message":"Connecting to NVD…","start":"","end":"","fetched":"","entries":[]})";
  IMPLEMENT_REFCOUNTING(CveService);
};
}
