#include "resources.h"
#include <fstream>
#include <sstream>
#include "include/cef_parser.h"
#include "include/wrapper/cef_stream_resource_handler.h"
namespace opengod {
namespace {
class Resources final : public CefSchemeHandlerFactory {
 public:
  explicit Resources(std::filesystem::path root) : root_(std::move(root)) {}
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser>,
                                       CefRefPtr<CefFrame>,
                                       const CefString&,
                                       CefRefPtr<CefRequest> request) override {
    CefURLParts parts;
    if (!CefParseURL(request->GetURL(), parts))
      return nullptr;
    const auto host = CefString(&parts.host).ToString();
    auto path = CefString(&parts.path).ToString();
    if (host != "shell" && host != "newtab" && host != "terminal")
      return nullptr;
    if (path.empty() || path == "/")
      path = host == "shell" ? "/index.html" : host == "terminal" ? "/terminal.html" : "/newtab.html";
    // Serve only packaged assets; no URL decoding or filesystem traversal.
    if (path.find("..") != std::string::npos || path.find('%') != std::string::npos ||
        path.find('\\') != std::string::npos || path.find(':') != std::string::npos)
      return nullptr;
    const auto file = root_ / path.substr(1);
    const auto ext = file.extension().string();
    std::string mime;
    if (ext == ".html")
      mime = "text/html";
    else if (ext == ".js")
      mime = "text/javascript";
    else if (ext == ".css")
      mime = "text/css";
    else if (ext == ".svg")
      mime = "image/svg+xml";
    else
      return nullptr;
    auto stream = CefStreamReader::CreateForFile(file.wstring());
    if (!stream)
      return nullptr;
    CefResponse::HeaderMap headers;
    headers.emplace("Content-Security-Policy",
                    host == "terminal"
                      ? "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; font-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'"
                      : "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
    headers.emplace("X-Content-Type-Options", "nosniff");
    return new CefStreamResourceHandler(200, "OK", mime, headers, stream);
  }

 private:
  std::filesystem::path root_;
  IMPLEMENT_REFCOUNTING(Resources);
};
}  // namespace
void register_resources(const std::filesystem::path& directory) {
  CefRegisterSchemeHandlerFactory("opengod", "shell", new Resources(directory));
  CefRegisterSchemeHandlerFactory("opengod", "newtab", new Resources(directory));
  CefRegisterSchemeHandlerFactory("opengod", "terminal", new Resources(directory));
}
}  // namespace opengod
