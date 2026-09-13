#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "core/browser_state.h"
#include "core/navigation.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_id_mappers.h"
#include "include/cef_parser.h"
#include "include/cef_sandbox_win.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "resources.h"
#include "cve_service.h"
#include "terminal/session.h"
#include "terminal_clipboard.h"
#include <charconv>
#include "tab_audio.h"
#include "window_frame.h"

namespace aurora {
namespace {
constexpr wchar_t kWindowClass[] = L"AuroraBrowserWindow";
constexpr char kShellUrl[] = "aurora://shell/index.html";
class Host;
Host* host = nullptr;  // UI-thread only; lifetime encloses the CEF message loop.

class Client final : public CefClient,
                     public CefLifeSpanHandler,
                     public CefDisplayHandler,
                     public CefLoadHandler,
                     public CefRequestHandler,
                     public CefDownloadHandler,
                     public CefResourceRequestHandler,
                     public CefKeyboardHandler,
                     public CefFocusHandler,
                     public CefCommandHandler,
                     public CefDragHandler,
                     public CefPermissionHandler,
                     public CefMessageRouterBrowserSide::Handler {
 public:
  Client(Host& owner, uint64_t tab, bool shell, bool devtools = false);
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }
  bool OnSetFocus(CefRefPtr<CefBrowser>, FocusSource) override;
  CefRefPtr<CefCommandHandler> GetCommandHandler() override { return this; }
  CefRefPtr<CefDragHandler> GetDragHandler() override { return this; }
  void OnDraggableRegionsChanged(CefRefPtr<CefBrowser>,
                                 CefRefPtr<CefFrame>,
                                 const std::vector<CefDraggableRegion>&) override;
  CefRefPtr<CefAudioHandler> GetAudioHandler() override { return audio_; }
  bool OnChromeCommand(CefRefPtr<CefBrowser>, int, cef_window_open_disposition_t) override;
  bool OnPreKeyEvent(CefRefPtr<CefBrowser>, const CefKeyEvent&, CefEventHandle, bool*) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override {
    if (devtools_)
      return false;
    // Each tab owns a child HWND. Default handling closes the shared top-level
    // window, so complete this child's teardown explicitly instead.
    DestroyWindow(browser->GetHost()->GetWindowHandle());
    return true;
  }
  void OnBeforeDevToolsPopup(CefRefPtr<CefBrowser>,
                             CefWindowInfo&,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings&,
                             CefRefPtr<CefDictionaryValue>&,
                             bool*) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override;
  void OnAddressChange(CefRefPtr<CefBrowser>,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool,
                      bool) override;
  bool OnBeforePopup(CefRefPtr<CefBrowser>,
                     CefRefPtr<CefFrame>,
                     int,
                     const CefString&,
                     const CefString&,
                     CefLifeSpanHandler::WindowOpenDisposition,
                     bool,
                     const CefPopupFeatures&,
                     CefWindowInfo&,
                     CefRefPtr<CefClient>&,
                     CefBrowserSettings&,
                     CefRefPtr<CefDictionaryValue>&,
                     bool*) override {
    return true;
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source,
                                CefRefPtr<CefProcessMessage> message) override {
    return !devtools_ && router_->OnProcessMessageReceived(browser, frame, source, message);
  }
  bool OnQuery(CefRefPtr<CefBrowser>,
               CefRefPtr<CefFrame>,
               int64_t,
               const CefString&,
               bool,
               CefRefPtr<Callback>) override;
  void stop_terminal();
  void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int) override;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus,
                                 int,
                                 const CefString&) override {
    stop_terminal();
    router_->OnRenderProcessTerminated(browser);
    if (audio_)
      audio_->OnAudioStreamStopped(browser);
    OnTitleChange(browser, "Tab stopped — reload to recover");
  }
  bool OnBeforeDownload(CefRefPtr<CefBrowser>,
                        CefRefPtr<CefDownloadItem>,
                        const CefString&,
                        CefRefPtr<CefBeforeDownloadCallback>) override {
    return true;
  }
  bool OnShowPermissionPrompt(CefRefPtr<CefBrowser>,
                              uint64_t,
                              const CefString&,
                              uint32_t,
                              CefRefPtr<CefPermissionPromptCallback> callback) override {
    callback->Continue(CEF_PERMISSION_RESULT_DENY);
    return true;
  }
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(CefRefPtr<CefBrowser>,
                                                                 CefRefPtr<CefFrame>,
                                                                 CefRefPtr<CefRequest>,
                                                                 bool,
                                                                 bool,
                                                                 const CefString&,
                                                                 bool&) override {
    return this;
  }
  void OnProtocolExecution(CefRefPtr<CefBrowser>,
                           CefRefPtr<CefFrame>,
                           CefRefPtr<CefRequest>,
                           bool& allow_os_execution) override {
    allow_os_execution = false;
  }

 private:
  Host& owner_;
  uint64_t tab_;
  bool shell_;
  bool devtools_;
  bool terminal_loaded_ = false;
  CefRefPtr<TabAudio> audio_;
  CefRefPtr<CefMessageRouterBrowserSide> router_;
  IMPLEMENT_REFCOUNTING(Client);
};

class Host {
 public:
  HWND window = nullptr;
  BrowserState state;
  struct TerminalTab { std::shared_ptr<terminal::Session> session; uint64_t generation; };
  std::map<uint64_t,TerminalTab> terminals;
  uint64_t next_terminal_generation=1;
  std::vector<std::shared_ptr<terminal::Session>> retired_terminals;
  bool confirm_terminal_close(uint64_t id) {
    const auto it=terminals.find(id);
    if(it==terminals.end())return true;
    if(!it->second.session->finished() && MessageBoxW(window,L"Close this terminal and stop its shell and all child processes? Unsaved work will be lost.",L"Close terminal",MB_OKCANCEL|MB_ICONWARNING|MB_DEFBUTTON2)!=IDOK)return false;
    it->second.session->close();return true;
  }
  void create_terminal() {
    if(terminals.size()>=8){MessageBoxW(window,L"Close a terminal before opening another (maximum eight).",L"Aurora",MB_OK);return;}
    const auto id=state.create_terminal();if(!id)return;
    terminals.emplace(id,TerminalTab{std::make_shared<terminal::Session>(),next_terminal_generation++});
    create_view(id,"aurora://terminal/");
  }
  CefRefPtr<CveService> cve_feed = new CveService;
  CefRefPtr<CefBrowser> shell;
  std::map<uint64_t, CefRefPtr<CefBrowser>> browsers;
  std::map<uint64_t, CefRefPtr<TabAudio>> audio;
  std::map<int, CefRefPtr<CefBrowser>> devtools;
  std::set<uint64_t> pending_tabs;
  std::set<uint64_t> pending_devtools;
  bool pending_shell = false;
  bool closing = false;
  int pending = 0;

  int toolbar_height() const { return MulDiv(140, static_cast<int>(GetDpiForWindow(window)), 96); }
  void layout() {
    RECT area{};
    GetClientRect(window, &area);
    const int top = toolbar_height();
    const int edge = frame_inset(window);
    const int width = std::max(0L, area.right - 2 * edge);
    if (shell)
      MoveWindow(shell->GetHost()->GetWindowHandle(), edge, edge, width, top, TRUE);
    for (const auto& [id, browser] : browsers) {
      auto handle = browser->GetHost()->GetWindowHandle();
      MoveWindow(handle, edge, top + edge, width, std::max(0L, area.bottom - top - 2 * edge), TRUE);
      ShowWindow(handle, id == state.active_tab() ? SW_SHOW : SW_HIDE);
    }
  }
  void create_view(uint64_t id, const std::string& url, bool chrome = false) {
    if (closing ||
        (chrome ? (shell || pending_shell) : (browsers.contains(id) || pending_tabs.contains(id))))
      return;
    CefWindowInfo info;
    // The pinned CEF PCM capture API is implemented by the Alloy browser host.
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    RECT area{};
    GetClientRect(window, &area);
    info.SetAsChild(
        window, CefRect(0, chrome ? 0 : toolbar_height(), area.right,
                        chrome ? toolbar_height() : std::max(0L, area.bottom - toolbar_height())));
    CefBrowserSettings settings;
    settings.background_color = CefColorSetARGB(255, 18, 24, 34);
    ++pending;
    if (chrome)
      pending_shell = true;
    else
      pending_tabs.insert(id);
    if (!CefBrowserHost::CreateBrowser(info, new Client(*this, id, chrome), url, settings, nullptr,
                                       nullptr)) {
      --pending;
      if (chrome)
        pending_shell = false;
      else
        pending_tabs.erase(id);
      if (!chrome)
        state.close_tab(id);
      MessageBoxW(window, L"Chromium could not create a browser view.", L"Aurora", MB_ICONERROR);
    }
  }
  void focus_new_tab_address(uint64_t id) {
    if (closing || state.active_tab() != id || terminals.contains(id) || !shell ||
        shell->GetMainFrame()->GetURL() != kShellUrl)
      return;
    shell->GetHost()->SetFocus(true);
    shell->GetMainFrame()->ExecuteJavaScript(
        "window.dispatchEvent(new CustomEvent('aurora-new-tab-address',{detail:" +
            std::to_string(id) + "}))", kShellUrl, 0);
  }
  void create_tab(const std::string& url) {
    const auto id = state.create_tab(url);
    if (id)
      create_view(id, state.tabs().back().url);
  }
  void synchronize() {
    // Creating a view can fail synchronously and remove its tab; iterate a copy.
    const auto tabs = state.tabs();
    for (const auto& tab : tabs) {
      if (!browsers.contains(tab.id) && !pending_tabs.contains(tab.id))
        create_view(tab.id, tab.url);
    }
    layout();
  }
  std::string snapshot() const {
    auto result = CefDictionaryValue::Create();
    result->SetInt("version", 1);
    result->SetInt("activeTab", static_cast<int>(state.active_tab()));
    auto window_state = CefDictionaryValue::Create();
    window_state->SetBool("maximized", IsZoomed(window) != FALSE);
    result->SetDictionary("window", window_state);
    auto tabs = CefListValue::Create();
    size_t index = 0;
    for (const auto& tab : state.tabs()) {
      auto entry = CefDictionaryValue::Create();
      entry->SetInt("id", static_cast<int>(tab.id));
      entry->SetString("url", tab.url);
      const auto terminal_it=terminals.find(tab.id);
      entry->SetString("title",terminal_it!=terminals.end()?"Terminal · "+terminal_it->second.session->inspect().status:tab.title);
      entry->SetBool("terminal",tab.terminal);
      entry->SetBool("active", tab.id == state.active_tab());
      entry->SetBool("muted", tab.muted);
      entry->SetInt("volume", tab.volume);
      const auto audio_it = audio.find(tab.id);
      if (audio_it != audio.end())
        entry->SetString("audioError", audio_it->second->error());
      tabs->SetDictionary(index++, entry);
    }
    result->SetList("tabs", tabs);
    auto bookmarks = CefListValue::Create();
    size_t bookmark_index = 0;
    for (const auto& bookmark : state.bookmarks()) {
      auto entry = CefDictionaryValue::Create();
      entry->SetInt("id", static_cast<int>(bookmark.id));
      entry->SetString("title", bookmark.title);
      entry->SetString("url", bookmark.url);
      bookmarks->SetDictionary(bookmark_index++, entry);
    }
    result->SetList("bookmarks", bookmarks);
    auto value = CefValue::Create();
    value->SetDictionary(result);
    return CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
  }
  void finish_close() {
    std::erase_if(retired_terminals,[](const auto& session){return session->finished();});
    if(!retired_terminals.empty())return;
    if (window && closing && pending == 0 && browsers.empty() && devtools.empty() && !shell) {
      const auto handle = window;
      window = nullptr;
      DestroyWindow(handle);
      CefQuitMessageLoop();
    }
  }
  void close() {
    if (closing) return;
    for(const auto& [id,terminal] : terminals) {
      if(!terminal.session->finished() && MessageBoxW(window,L"Closing Aurora stops all terminal shells and child processes. Continue?",L"Close Aurora",MB_OKCANCEL|MB_ICONWARNING|MB_DEFBUTTON2)!=IDOK)return;
      if(!terminal.session->finished())break;
    }
    for(auto& [id,terminal] : terminals)terminal.session->close();
    cve_feed->shutdown();
    closing = true;
    for (auto& [id, output] : audio)
      output->close();
    const auto tools_to_close = devtools;
    const auto tabs_to_close = browsers;
    for (const auto& [id, browser] : tools_to_close)
      browser->GetHost()->CloseBrowser(true);
    if (shell)
      shell->GetHost()->CloseBrowser(true);
    for (const auto& [id, browser] : tabs_to_close)
      browser->GetHost()->CloseBrowser(true);
    finish_close();
  }
};

void Client::OnDraggableRegionsChanged(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_UI_THREAD();
  if (!shell_ || owner_.closing || !owner_.shell || !frame->IsMain() ||
      frame->GetURL() != kShellUrl || browser->GetIdentifier() != owner_.shell->GetIdentifier())
    return;
  std::vector<RECT> rectangles;
  const int dpi = static_cast<int>(GetDpiForWindow(owner_.window));
  const int edge = frame_inset(owner_.window);
  for (const auto& region : regions) {
    if (!region.draggable)
      continue;
    const auto& b = region.bounds;
    rectangles.push_back({edge + MulDiv(b.x, dpi, 96), edge + MulDiv(b.y, dpi, 96),
                          edge + MulDiv(b.x + b.width, dpi, 96),
                          edge + MulDiv(b.y + b.height, dpi, 96)});
  }
  set_drag_regions(owner_.window, browser->GetHost()->GetWindowHandle(), rectangles);
}

void Client::OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int) {
  if (!frame->IsMain()) return;
  terminal_loaded_ = true;
  // Startup can create the content view before the shell document is ready.
  if (shell_) owner_.focus_new_tab_address(owner_.state.active_tab());
}
bool Client::OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) {
  if (shell_ || devtools_ || source != FOCUS_SOURCE_NAVIGATION) return false;
  const auto url = browser->GetMainFrame()->GetURL().ToString();
  // Loading the starting page must not steal focus back from the address bar.
  // User clicks and terminal focus requests retain normal CEF behavior.
  return url == "aurora://newtab" || url == "aurora://newtab/";
}
void Client::stop_terminal() { if(shell_||devtools_)return; const auto it=owner_.terminals.find(tab_);if(it!=owner_.terminals.end())it->second.session->close(); }
Client::Client(Host& owner, uint64_t tab, bool shell, bool devtools)
    : owner_(owner), tab_(tab), shell_(shell), devtools_(devtools) {
  router_ = CefMessageRouterBrowserSide::Create(CefMessageRouterConfig{});
  if (!devtools_)
    router_->AddHandler(this, false);
  if (!shell_ && !devtools_) {
    audio_ = new TabAudio;
    for (const auto& state_tab : owner_.state.tabs()) {
      if (state_tab.id == tab_)
        audio_->set_settings(state_tab.muted, state_tab.volume);
    }
  }
}
bool Client::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                           const CefKeyEvent& event,
                           CefEventHandle,
                           bool*) {
  CEF_REQUIRE_UI_THREAD();
  if (devtools_ || owner_.closing || event.type != KEYEVENT_RAWKEYDOWN)
    return false;
  const auto target_id = shell_ ? owner_.state.active_tab() : tab_;
  if (shell_) {
    const auto active = owner_.browsers.find(target_id);
    browser = active == owner_.browsers.end() ? nullptr : active->second;
  }
  const bool control = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
  const bool alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;
  const bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
  if (!shell_ && owner_.terminals.contains(tab_) && !(control && shift && event.windows_key_code=='L')) return false;
  if (control && !alt) {
    switch (event.windows_key_code) {
      case 'L':
      case 'K': {
        if (!owner_.shell || owner_.shell->GetMainFrame()->GetURL() != kShellUrl)
          return false;
        owner_.shell->GetHost()->SetFocus(true);
        owner_.shell->GetMainFrame()->ExecuteJavaScript(
            event.windows_key_code == 'L'
                ? "window.dispatchEvent(new Event('aurora-focus-address'))"
                : "window.dispatchEvent(new Event('aurora-open-commands'))",
            kShellUrl, 0);
        return true;
      }
      case 'T':
        if (shift) {
          (void)owner_.state.reopen_closed_tab();
          owner_.synchronize();
        } else
          owner_.create_tab("aurora://newtab");
        owner_.layout();
        return true;
      case 'W':
        if (!owner_.confirm_terminal_close(target_id)) return true;
        if (const auto output = owner_.audio.find(target_id); output != owner_.audio.end())
          output->second->close();
        if (owner_.state.close_tab(target_id)) {
          if (browser)
            browser->GetHost()->CloseBrowser(true);
          if (owner_.state.tabs().empty())
            owner_.create_tab("aurora://newtab");
          owner_.layout();
        }
        return true;
      case 'R':
        if (browser) {
          if (shift)
            browser->ReloadIgnoreCache();
          else
            browser->Reload();
        }
        return true;
      default:
        break;
    }
  } else if (alt && !control && !shift) {
    if (event.windows_key_code == VK_LEFT) {
      if (browser)
        browser->GoBack();
      return true;
    }
    if (event.windows_key_code == VK_RIGHT) {
      if (browser)
        browser->GoForward();
      return true;
    }
  }
  return false;
}
bool Client::OnChromeCommand(CefRefPtr<CefBrowser> browser,
                             int command,
                             cef_window_open_disposition_t) {
  CEF_REQUIRE_UI_THREAD();
  if (devtools_ || owner_.closing)
    return false;
  // Chrome-style accelerators can run before renderer key events. Route those
  // commands through the same Aurora actions so no hidden Chrome tab UI opens.
  struct Shortcut {
    const char* command;
    int key;
    uint32_t modifiers;
  };
  static constexpr Shortcut shortcuts[] = {
      {"IDC_FOCUS_LOCATION", 'L', EVENTFLAG_CONTROL_DOWN},
      {"IDC_FOCUS_SEARCH", 'K', EVENTFLAG_CONTROL_DOWN},
      {"IDC_NEW_TAB", 'T', EVENTFLAG_CONTROL_DOWN},
      {"IDC_RESTORE_TAB", 'T', EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN},
      {"IDC_CLOSE_TAB", 'W', EVENTFLAG_CONTROL_DOWN},
      {"IDC_RELOAD", 'R', EVENTFLAG_CONTROL_DOWN},
      {"IDC_RELOAD_BYPASSING_CACHE", 'R', EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN},
      {"IDC_BACK", VK_LEFT, EVENTFLAG_ALT_DOWN},
      {"IDC_FORWARD", VK_RIGHT, EVENTFLAG_ALT_DOWN}};
  for (const auto& shortcut : shortcuts) {
    if (command != cef_id_for_command_id_name(shortcut.command))
      continue;
    CefKeyEvent event;
    event.type = KEYEVENT_RAWKEYDOWN;
    event.windows_key_code = shortcut.key;
    event.modifiers = shortcut.modifiers;
    bool handled = false;
    (void)OnPreKeyEvent(browser, event, nullptr, &handled);
    return true;
  }
  return false;
}
void Client::OnBeforeDevToolsPopup(CefRefPtr<CefBrowser>,
                                   CefWindowInfo&,
                                   CefRefPtr<CefClient>& client,
                                   CefBrowserSettings&,
                                   CefRefPtr<CefDictionaryValue>&,
                                   bool*) {
  CEF_REQUIRE_UI_THREAD();
  // Covers both the Aurora command and Chromium's built-in menu/shortcut.
  if (owner_.pending_devtools.insert(tab_).second)
    ++owner_.pending;
  client = new Client(owner_, tab_, false, true);
}
void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  --owner_.pending;
  if (devtools_) {
    owner_.pending_devtools.erase(tab_);
    owner_.devtools.emplace(browser->GetIdentifier(), browser);
  } else if (shell_) {
    owner_.pending_shell = false;
    owner_.shell = browser;
  } else {
    owner_.pending_tabs.erase(tab_);
    owner_.browsers.emplace(tab_, browser);
    if (audio_) {
      owner_.audio.emplace(tab_, audio_);
      // Capture remains live; suppress Chromium's direct output so it cannot
      // bypass the per-tab native gain (including capture startup/recovery).
      browser->GetHost()->SetAudioMuted(true);
    }
  }
  const auto& tabs = owner_.state.tabs();
  const bool tab_exists =
      std::any_of(tabs.begin(), tabs.end(), [this](const auto& tab) { return tab.id == tab_; });
  if (owner_.closing || (!shell_ && !tab_exists))
    browser->GetHost()->CloseBrowser(true);
  else {
    owner_.layout();
    if (!shell_ && !devtools_) owner_.focus_new_tab_address(tab_);
  }
}
void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  stop_terminal();
  if(const auto it=owner_.terminals.find(tab_);!shell_&&!devtools_&&it!=owner_.terminals.end()) {owner_.retired_terminals.push_back(it->second.session);owner_.terminals.erase(it);}
  router_->OnBeforeClose(browser);
  if (audio_)
    audio_->close();
  if (devtools_)
    owner_.devtools.erase(browser->GetIdentifier());
  else if (shell_) {
    if (owner_.shell && owner_.shell->IsSame(browser))
      owner_.shell = nullptr;
    // Losing the privileged shell must also terminate its content children.
    owner_.close();
  } else {
    const auto it = owner_.browsers.find(tab_);
    if (it != owner_.browsers.end() && it->second->IsSame(browser)) {
      owner_.browsers.erase(it);
      owner_.audio.erase(tab_);
      owner_.state.close_tab(tab_);
    }
  }
  if (!owner_.closing)
    owner_.layout();
  owner_.finish_close();
}
void Client::OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) {
  if (!shell_ && !devtools_)
    owner_.state.set_title(tab_, title.ToString());
}
void Client::OnAddressChange(CefRefPtr<CefBrowser>,
                             CefRefPtr<CefFrame> frame,
                             const CefString& url) {
  if (!shell_ && !devtools_ && frame->IsMain())
    owner_.state.navigate(tab_, url.ToString());
}
bool Client::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            bool,
                            bool) {
  router_->OnBeforeBrowse(browser, frame);
  const auto url = request->GetURL().ToString();
  if (devtools_)
    return false;  // Separate client: no product state or command bridge.
  if (shell_)
    return !frame->IsMain() || url != kShellUrl;
  if (owner_.terminals.contains(tab_)) {
    if (!frame->IsMain() || url != "aurora://terminal/") return true;
    if (terminal_loaded_) stop_terminal();
    return false;
  }
  if (url.starts_with("aurora://terminal")) return true;
  if (!frame->IsMain())
    return url.starts_with("aurora:");
  const auto target = resolve_navigation(url);
  return !target.allowed || target.url.starts_with("aurora://shell") ||
         target.url == "aurora://settings";
}
bool Client::OnQuery(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     int64_t,
                     const CefString& request,
                     bool persistent,
                     CefRefPtr<Callback> callback) {
  CEF_REQUIRE_UI_THREAD();
  if (!shell_ && owner_.terminals.contains(tab_)) {
    const auto found=owner_.browsers.find(tab_);
    auto fail=[&](const char* message){callback->Failure(400,message);return true;};
    if(devtools_||owner_.closing||found==owner_.browsers.end()||found->second->GetIdentifier()!=browser->GetIdentifier()||!frame->IsMain()||frame->GetURL()!="aurora://terminal/"||persistent||request.length()>32768)return fail("Unauthorized terminal request");
    auto value=CefParseJSON(request,JSON_PARSER_RFC);auto d=value&&value->GetType()==VTYPE_DICTIONARY?value->GetDictionary():nullptr;
    if(!d||d->GetType("version")!=VTYPE_INT||d->GetInt("version")!=1||d->GetType("command")!=VTYPE_STRING)return fail("Invalid terminal envelope");
    const auto command=d->GetString("command").ToString();auto& term=owner_.terminals.at(tab_);
    const bool hello=command=="terminalHello";
    if(!hello&&(d->GetType("session")!=VTYPE_STRING||d->GetString("session").ToString()!=std::to_string(term.generation)))return fail("Stale terminal session");
    uint64_t cursor=0;
    if(command=="terminalRead") {
      if(d->GetSize()!=4||d->GetType("cursor")!=VTYPE_STRING)return fail("Invalid terminal cursor");
      auto text=d->GetString("cursor").ToString();auto result=std::from_chars(text.data(),text.data()+text.size(),cursor);
      if(text.empty()||result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return fail("Invalid terminal cursor");
    } else if(command=="terminalInput") {
      if(d->GetSize()!=4||d->GetType("data")!=VTYPE_STRING||!term.session->input(d->GetString("data").ToString()))return fail("Terminal input rejected or buffer full");
      callback->Success("{}");return true;
    } else if(command=="terminalCopy"||command=="terminalPaste") {
      const bool copy=command=="terminalCopy";
      if(d->GetSize()!=(copy?4u:3u)||(copy&&d->GetType("data")!=VTYPE_STRING))return fail("Invalid clipboard command");
      auto text=terminal_clipboard(owner_.window,copy?std::optional<std::wstring>(d->GetString("data").ToWString()):std::nullopt);
      if(!text)return fail("Clipboard unavailable or exceeds 32 KiB of text; focus Aurora and retry");
      auto out=CefDictionaryValue::Create();out->SetString("text",*text);auto response=CefValue::Create();response->SetDictionary(out);callback->Success(CefWriteJSON(response,JSON_WRITER_DEFAULT));return true;
    } else if(command=="terminalResize") {
      if(d->GetSize()!=5||d->GetType("columns")!=VTYPE_INT||d->GetType("rows")!=VTYPE_INT||!term.session->resize(d->GetInt("columns"),d->GetInt("rows")))return fail("Invalid terminal dimensions");
      callback->Success("{}");return true;
    } else if(command=="terminalRestart") {
      if(d->GetSize()!=3||!term.session->finished())return fail("Wait for the previous shell to exit");
      term.session=std::make_shared<terminal::Session>();term.generation=owner_.next_terminal_generation++;
    } else if(!hello||d->GetSize()!=2)return fail("Unknown terminal command");
    auto snapshot=hello?std::optional<terminal::Snapshot>(term.session->inspect()):term.session->read(cursor);if(!snapshot)return fail("Invalid output acknowledgement; reload to stop and restart");
    auto out=CefDictionaryValue::Create();out->SetInt("version",1);out->SetString("session",std::to_string(term.generation));out->SetString("status",snapshot->status);out->SetString("shell",snapshot->shell);out->SetString("error",snapshot->error);out->SetString("cursor",std::to_string(snapshot->next));out->SetString("data",CefBase64Encode(snapshot->bytes.data(),snapshot->bytes.size()));
    auto response=CefValue::Create();response->SetDictionary(out);callback->Success(CefWriteJSON(response,JSON_WRITER_DEFAULT));return true;
  }
  if (!shell_) {
    const auto found = owner_.browsers.find(tab_);
    const auto url = frame->GetURL().ToString();
    if (devtools_ || owner_.closing || found == owner_.browsers.end() ||
        found->second->GetIdentifier() != browser->GetIdentifier() || !frame->IsMain() ||
        (url != "aurora://newtab/" && url != "aurora://newtab") || persistent || request.length() > 256) {
      callback->Failure(403, "Untrusted feed request"); return true;
    }
    auto value = CefParseJSON(request, JSON_PARSER_RFC);
    auto d = value && value->GetType() == VTYPE_DICTIONARY ? value->GetDictionary() : nullptr;
    if (!d || d->GetSize() != 2 || d->GetType("version") != VTYPE_INT || d->GetInt("version") != 1 ||
        d->GetType("command") != VTYPE_STRING ||
        (d->GetString("command") != "cveState" && d->GetString("command") != "refreshCves" && d->GetString("command") != "createTerminal")) {
      callback->Failure(400, "Invalid read-only feed command"); return true;
    }
    if(d->GetString("command")=="createTerminal"){owner_.create_terminal();owner_.layout();callback->Success("{}");return true;}
    owner_.cve_feed->poll(d->GetString("command") == "refreshCves");
    callback->Success(owner_.cve_feed->snapshot()); return true;
  }
  if (!shell_ || !owner_.shell || browser->GetIdentifier() != owner_.shell->GetIdentifier() ||
      !frame->IsMain() || frame->GetURL() != kShellUrl || persistent || request.length() > 16384) {
    callback->Failure(403, "Untrusted request");
    return true;
  }
  auto value = CefParseJSON(request, JSON_PARSER_RFC);
  auto fail = [&](const char* error) {
    callback->Failure(400, error);
    return true;
  };
  if (!value || value->GetType() != VTYPE_DICTIONARY)
    return fail("Expected object");
  auto msg = value->GetDictionary();
  if (msg->GetType("version") != VTYPE_INT || msg->GetInt("version") != 1 ||
      msg->GetType("command") != VTYPE_STRING)
    return fail("Invalid protocol");
  CefDictionaryValue::KeyList keys;
  msg->GetKeys(keys);
  for (const auto& key : keys) {
    if (key != "version" && key != "command" && key != "tabId" && key != "url" && key != "muted" &&
        key != "volume" && key != "bookmarkId")
      return fail("Unknown field");
  }
  if (msg->HasKey("tabId") && (msg->GetType("tabId") != VTYPE_INT || msg->GetInt("tabId") <= 0))
    return fail("Invalid tab ID");
  if (msg->HasKey("url") && msg->GetType("url") != VTYPE_STRING)
    return fail("Invalid URL");
  const auto command = msg->GetString("command").ToString();
  if (msg->HasKey("bookmarkId") &&
      (command != "removeBookmark" || msg->GetType("bookmarkId") != VTYPE_INT ||
       msg->GetInt("bookmarkId") <= 0))
    return fail("Invalid bookmark ID");
  if (msg->HasKey("muted") && (command != "setTabMuted" || msg->GetType("muted") != VTYPE_BOOL))
    return fail("Invalid mute argument");
  if (msg->HasKey("volume") && (command != "setTabVolume" || msg->GetType("volume") != VTYPE_INT ||
                                msg->GetInt("volume") < 0 || msg->GetInt("volume") > 100))
    return fail("Volume must be an integer from 0 to 100");
  if ((command == "setTabMuted" || command == "setTabVolume") &&
      (!msg->HasKey("tabId") || !msg->HasKey(command == "setTabMuted" ? "muted" : "volume")))
    return fail("Audio commands require an explicit tab and value");
  if (command == "removeBookmark" && !msg->HasKey("bookmarkId"))
    return fail("Bookmark commands require a bookmark ID");
  const auto id = msg->HasKey("tabId") ? static_cast<uint64_t>(msg->GetInt("tabId"))
                                       : owner_.state.active_tab();
  auto it = owner_.browsers.find(id);
  if (owner_.closing)
    return fail("Window closing");
  if (command == "state") { /* Read-only snapshot. */
  } else if (command == "minimizeWindow") {
    ShowWindow(owner_.window, SW_MINIMIZE);
  } else if (command == "toggleMaximize") {
    ShowWindow(owner_.window, IsZoomed(owner_.window) ? SW_RESTORE : SW_MAXIMIZE);
  } else if (command == "closeWindow") {
    callback->Success(owner_.snapshot());
    PostMessageW(owner_.window, WM_CLOSE, 0, 0);
    return true;
  } else if (command == "createTab") {
    const auto url = msg->HasKey("url") ? msg->GetString("url").ToString() : "aurora://newtab";
    if (!resolve_navigation(url).allowed)
      return fail("Navigation denied");
    owner_.create_tab(url);
  } else if (command == "reopenTab") {
    const auto restored = owner_.state.reopen_closed_tab();
    if (!restored)
      return fail("No closed tab");
    owner_.synchronize();
  } else if (command == "removeBookmark") {
    if (!owner_.state.remove_bookmark(static_cast<uint64_t>(msg->GetInt("bookmarkId"))))
      return fail("Bookmark could not be removed");
  } else {
    const auto& open_tabs = owner_.state.tabs();
    if (it == owner_.browsers.end() || std::none_of(open_tabs.begin(), open_tabs.end(),
                                                    [id](const auto& tab) { return tab.id == id; }))
      return fail("Tab unavailable");
    if (command == "setTabMuted" || command == "setTabVolume") {
      const bool changed = command == "setTabMuted"
                               ? owner_.state.set_muted(id, msg->GetBool("muted"))
                               : owner_.state.set_volume(id, msg->GetInt("volume"));
      if (!changed)
        return fail("Audio setting rejected");
      const auto audio_it = owner_.audio.find(id);
      if (audio_it != owner_.audio.end()) {
        for (const auto& tab : owner_.state.tabs())
          if (tab.id == id)
            audio_it->second->set_settings(tab.muted, tab.volume);
      }
    } else if (command == "addBookmark") {
      if (!owner_.state.add_bookmark(id))
        return fail("This page cannot be bookmarked");
    } else if (command == "activateTab")
      owner_.state.activate_tab(id);
    else if (command == "closeTab") {
      if(!owner_.confirm_terminal_close(id))return fail("Terminal close canceled");
      const auto audio_it = owner_.audio.find(id);
      if (audio_it != owner_.audio.end())
        audio_it->second->close();
      owner_.state.close_tab(id);
      it->second->GetHost()->CloseBrowser(true);
      if (owner_.state.tabs().empty())
        owner_.create_tab("aurora://newtab");
    } else if (command == "duplicateTab") {
      if(owner_.terminals.contains(id))return fail("Open a new terminal instead of duplicating a session");
      (void)owner_.state.duplicate_tab(id);
      owner_.synchronize();
    } else if (command == "navigate") {
      if (!msg->HasKey("url") || !owner_.state.navigate(id, msg->GetString("url").ToString()))
        return fail("Navigation denied");
      const auto& tabs = owner_.state.tabs();
      auto tab = std::find_if(tabs.begin(), tabs.end(), [id](const auto& t) { return t.id == id; });
      it->second->GetMainFrame()->LoadURL(tab->url);
    } else if (command == "back")
      it->second->GoBack();
    else if (command == "forward")
      it->second->GoForward();
    else if (command == "reload")
      it->second->Reload();
    else if (command == "devtools") {
      if (owner_.pending_devtools.contains(id))
        return fail("Developer tools are opening");
      CefWindowInfo info;
      info.SetAsPopup(owner_.window, "Aurora Developer Tools");
      if (!it->second->GetHost()->HasDevTools() && owner_.pending_devtools.insert(id).second)
        ++owner_.pending;
      it->second->GetHost()->ShowDevTools(info, new Client(owner_, id, false, true),
                                          CefBrowserSettings{}, CefPoint{});
    } else
      return fail("Unknown command");
  }
  owner_.layout();
  callback->Success(owner_.snapshot());
  return true;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto result = handle_frame_message(window, message, wparam, lparam))
    return *result;
  if (host) {
    if(message==WM_TIMER && wparam==1){host->finish_close();return 0;}
    if (message == WM_SIZE) {
      host->layout();
      return 0;
    }
    if (message == WM_CLOSE) {
      host->close();
      return 0;
    }
    if (message == WM_DPICHANGED) {
      const auto* rect = reinterpret_cast<RECT*>(lparam);
      SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                   rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
      host->layout();
      return 0;
    }
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

class App final : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler {
 public:
  explicit App(std::filesystem::path assets) : assets_(std::move(assets)) {}
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
    registrar->AddCustomScheme("aurora", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE);
  }
  void OnContextInitialized() override {
    register_resources(assets_);
    host->create_view(0, kShellUrl, true);
    host->create_tab("aurora://newtab");
  }
  void OnWebKitInitialized() override {
    renderer_ = CefMessageRouterRendererSide::Create(CefMessageRouterConfig{});
  }
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    if (frame->IsMain() && (frame->GetURL() == kShellUrl || frame->GetURL() == "aurora://newtab/" || frame->GetURL() == "aurora://newtab" || frame->GetURL() == "aurora://terminal/"))
      renderer_->OnContextCreated(browser, frame, context);
  }
  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
    if (renderer_)
      renderer_->OnContextReleased(browser, frame, context);
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source,
                                CefRefPtr<CefProcessMessage> message) override {
    return renderer_ && renderer_->OnProcessMessageReceived(browser, frame, source, message);
  }

 private:
  std::filesystem::path assets_;
  CefRefPtr<CefMessageRouterRendererSide> renderer_;
  IMPLEMENT_REFCOUNTING(App);
};
}  // namespace
}  // namespace aurora

extern "C" __declspec(dllexport) int RunWinMain(HINSTANCE instance,
                                                LPWSTR,
                                                int show,
                                                void* sandbox_info,
                                                cef_version_info_t*) {
  if (!sandbox_info)
    return 2;
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  const auto directory = std::filesystem::path(executable).parent_path();
  CefMainArgs args(instance);
  CefRefPtr<aurora::App> app = new aurora::App(directory / "ui");
  const int result = CefExecuteProcess(args, app, sandbox_info);
  if (result >= 0)
    return result;
  auto command = CefCommandLine::CreateCommandLine();
  command->InitFromString(GetCommandLineW());
  if (command->HasSwitch("no-sandbox") || command->HasSwitch("disable-web-security") ||
      command->HasSwitch("ignore-certificate-errors"))
    return 3;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  aurora::Host owner;
  aurora::host = &owner;
  WNDCLASSW wc{};
  wc.lpfnWndProc = aurora::WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = aurora::kWindowClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&wc);
  owner.window = CreateWindowExW(
      0, aurora::kWindowClass, L"Aurora",
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
      CW_USEDEFAULT, CW_USEDEFAULT, 1360, 900, nullptr, nullptr, instance, nullptr);
  if (!owner.window)
    return 4;
  SetWindowPos(owner.window, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  CefSettings settings;
  settings.no_sandbox = false;
  settings.log_severity = LOGSEVERITY_DISABLE;
  // OS-managed user profile location, separate from the install and source trees.
  wchar_t local[MAX_PATH]{};
  if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH))
    return 5;
  CefString(&settings.root_cache_path) =
      (std::filesystem::path(local) / "Aurora" / "Profiles").wstring();
  CefString(&settings.cache_path) =
      (std::filesystem::path(local) / "Aurora" / "Profiles" / "Default").wstring();
  if (!CefInitialize(args, settings, app, sandbox_info))
    return 6;
  SetTimer(owner.window,1,50,nullptr);
  ShowWindow(owner.window, show);
  CefRunMessageLoop();
  aurora::host = nullptr;
  CefShutdown();
  return 0;
}
