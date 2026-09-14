#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace opengod {

struct Tab {
  std::uint64_t id = 0;
  std::string url;
  std::string title;
  bool pinned = false;
  bool muted = false;
  std::uint64_t workspace_id = 1;
  int volume = 100;
  bool terminal = false;
};

struct Workspace {
  std::uint64_t id;
  std::string name;
};

struct Bookmark {
  std::uint64_t id = 0;
  std::string title;
  std::string url;
};

struct Macro {
  std::uint64_t id = 0;
  std::string name;
  std::string terminal_command;
  std::string url;
};

// UI-thread owned state. Adapters must marshal every operation onto that thread.
// IDs are monotonic and never recycled, including restored/duplicated tabs.
class BrowserState {
 public:
  [[nodiscard]] std::uint64_t create_tab(std::string input = "opengod://newtab");
  [[nodiscard]] std::uint64_t create_terminal();
  bool close_tab(std::uint64_t id);
  bool activate_tab(std::uint64_t id);
  bool navigate(std::uint64_t id, std::string input);
  [[nodiscard]] std::uint64_t duplicate_tab(std::uint64_t id);
  [[nodiscard]] std::uint64_t reopen_closed_tab();
  bool set_pinned(std::uint64_t id, bool pinned);
  bool set_muted(std::uint64_t id, bool muted);
  bool set_volume(std::uint64_t id, int volume);
  bool set_title(std::uint64_t id, std::string title);
  // Bookmarks form a session-lifetime collection independent of open tabs.
  // Persistence is a later profile milestone.
  [[nodiscard]] std::uint64_t add_bookmark(std::uint64_t tab_id);
  bool remove_bookmark(std::uint64_t bookmark_id);
  [[nodiscard]] const std::vector<Bookmark>& bookmarks() const { return bookmarks_; }
  // Macros are in-memory, session-lifetime (documented limitation).
  [[nodiscard]] std::uint64_t add_macro(std::string name, std::string terminal_command, std::string url);
  bool update_macro(std::uint64_t id, std::string name, std::string terminal_command, std::string url);
  bool remove_macro(std::uint64_t macro_id);
  [[nodiscard]] const std::vector<Macro>& macros() const { return macros_; }
  [[nodiscard]] std::uint64_t create_workspace(std::string name);
  bool activate_workspace(std::uint64_t id);
  bool move_to_workspace(std::uint64_t tab_id, std::uint64_t workspace_id);
  [[nodiscard]] const std::vector<Tab>& tabs() const { return tabs_; }
  [[nodiscard]] const std::vector<Workspace>& workspaces() const { return workspaces_; }
  [[nodiscard]] std::uint64_t active_tab() const { return active_tab_; }
  [[nodiscard]] std::uint64_t active_workspace() const { return active_workspace_; }
  [[nodiscard]] const std::string& last_error() const { return last_error_; }

 private:
  Tab* find(std::uint64_t id);
  void select_workspace_fallback();
  std::vector<Tab> tabs_;
  std::vector<Tab> closed_tabs_;
  std::vector<Workspace> workspaces_{{1, "Personal"}};
  std::vector<Bookmark> bookmarks_;
  std::vector<Macro> macros_;
  std::uint64_t next_tab_id_ = 1;
  std::uint64_t next_workspace_id_ = 2;
  std::uint64_t next_bookmark_id_ = 1;
  std::uint64_t next_macro_id_ = 1;
  std::uint64_t active_tab_ = 0;
  std::uint64_t active_workspace_ = 1;
  std::string last_error_;
};

}  // namespace opengod
