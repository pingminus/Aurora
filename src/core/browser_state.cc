#include "core/browser_state.h"
#include "core/navigation.h"

#include <algorithm>
#include <utility>

namespace aurora {
Tab* BrowserState::find(std::uint64_t id) {
  const auto it =
      std::find_if(tabs_.begin(), tabs_.end(), [id](const Tab& t) { return t.id == id; });
  return it == tabs_.end() ? nullptr : &*it;
}

std::uint64_t BrowserState::create_tab(std::string input) {
  auto target = resolve_navigation(input);
  last_error_ = target.error;
  if (!target.allowed)
    return 0;
  const auto id = next_tab_id_++;
  tabs_.push_back({id, std::move(target.url), "New tab", false, false, active_workspace_});
  active_tab_ = id;
  return id;
}

std::uint64_t BrowserState::create_terminal() {
 const auto id=create_tab();auto* tab=find(id);if(!tab)return 0;
 tab->terminal=true;tab->url="aurora://terminal/";tab->title="Terminal";return id;
}
void BrowserState::select_workspace_fallback() {
  const auto it = std::find_if(tabs_.begin(), tabs_.end(), [this](const Tab& t) {
    return t.workspace_id == active_workspace_;
  });
  active_tab_ = it == tabs_.end() ? 0 : it->id;
}

bool BrowserState::close_tab(std::uint64_t id) {
  const auto it =
      std::find_if(tabs_.begin(), tabs_.end(), [id](const Tab& t) { return t.id == id; });
  if (it == tabs_.end())
    return false;
  const auto index = static_cast<std::size_t>(it - tabs_.begin());
  if (!it->terminal) closed_tabs_.push_back(*it);
  if (closed_tabs_.size() > 25)
    closed_tabs_.erase(closed_tabs_.begin());
  tabs_.erase(it);
  if (active_tab_ == id) {
    select_workspace_fallback();
    // Prefer the neighboring tab in the same workspace.
    if (!tabs_.empty()) {
      const auto& neighbor = tabs_[std::min(index, tabs_.size() - 1)];
      if (neighbor.workspace_id == active_workspace_)
        active_tab_ = neighbor.id;
    }
  }
  return true;
}

bool BrowserState::activate_tab(std::uint64_t id) {
  const auto* tab = find(id);
  if (!tab)
    return false;
  active_tab_ = id;
  active_workspace_ = tab->workspace_id;
  return true;
}

bool BrowserState::navigate(std::uint64_t id, std::string input) {
  auto* tab = find(id);
  if (!tab)
    return false;
  if (tab->terminal) return input == "aurora://terminal/";
  auto target = resolve_navigation(input);
  last_error_ = target.error;
  if (!target.allowed)
    return false;
  tab->url = std::move(target.url);
  tab->title = tab->url;
  return true;
}

std::uint64_t BrowserState::duplicate_tab(std::uint64_t id) {
  const auto* original = find(id);
  if (!original || original->terminal)
    return 0;
  auto copy = *original;
  copy.id = next_tab_id_++;
  copy.pinned = false;
  copy.muted = false;
  copy.volume = 100;
  tabs_.push_back(copy);
  activate_tab(copy.id);
  return copy.id;
}

std::uint64_t BrowserState::reopen_closed_tab() {
  if (closed_tabs_.empty())
    return 0;
  auto tab = std::move(closed_tabs_.back());
  closed_tabs_.pop_back();
  tab.id = next_tab_id_++;
  tabs_.push_back(tab);
  activate_tab(tab.id);
  return tab.id;
}

bool BrowserState::set_pinned(std::uint64_t id, bool pinned) {
  auto* tab = find(id);
  if (!tab)
    return false;
  tab->pinned = pinned;
  return true;
}
bool BrowserState::set_muted(std::uint64_t id, bool muted) {
  auto* tab = find(id);
  if (!tab)
    return false;
  tab->muted = muted;
  return true;
}

bool BrowserState::set_volume(std::uint64_t id, int volume) {
  if (volume < 0 || volume > 100) return false;
  auto* tab = find(id);
  if (!tab) return false;
  tab->volume = volume;
  return true;
}
bool BrowserState::set_title(std::uint64_t id, std::string title) {
  auto* tab = find(id);
  if (!tab)
    return false;
  if (title.size() > 4096)
    title.resize(4096);
  tab->title = std::move(title);
  return true;
}
std::uint64_t BrowserState::add_bookmark(std::uint64_t tab_id) {
  const auto* tab = find(tab_id);
  if (!tab || tab->terminal || tab->url.empty() || tab->url.starts_with("aurora://"))
    return 0;
  const auto existing =
      std::find_if(bookmarks_.begin(), bookmarks_.end(),
                   [&tab](const Bookmark& bookmark) { return bookmark.url == tab->url; });
  if (existing != bookmarks_.end())
    return existing->id;
  const auto id = next_bookmark_id_++;
  bookmarks_.push_back({id, tab->title, tab->url});
  return id;
}
bool BrowserState::remove_bookmark(std::uint64_t bookmark_id) {
  const auto it =
      std::find_if(bookmarks_.begin(), bookmarks_.end(),
                   [bookmark_id](const Bookmark& bookmark) { return bookmark.id == bookmark_id; });
  if (it == bookmarks_.end())
    return false;
  bookmarks_.erase(it);
  return true;
}
std::uint64_t BrowserState::create_workspace(std::string name) {
  if (name.empty() || name.size() > 128)
    return 0;
  const auto id = next_workspace_id_++;
  workspaces_.push_back({id, std::move(name)});
  return id;
}
bool BrowserState::activate_workspace(std::uint64_t id) {
  if (std::none_of(workspaces_.begin(), workspaces_.end(),
                   [id](const Workspace& w) { return w.id == id; }))
    return false;
  if (id != active_workspace_) {
    active_workspace_ = id;
    select_workspace_fallback();
  }
  return true;
}
bool BrowserState::move_to_workspace(std::uint64_t tab_id, std::uint64_t workspace_id) {
  auto* tab = find(tab_id);
  if (!tab || std::none_of(workspaces_.begin(), workspaces_.end(),
                           [workspace_id](const Workspace& w) { return w.id == workspace_id; }))
    return false;
  tab->workspace_id = workspace_id;
  if (active_tab_ == tab_id && workspace_id != active_workspace_)
    select_workspace_fallback();
  if (active_tab_ == 0 && workspace_id == active_workspace_)
    active_tab_ = tab_id;
  return true;
}
}  // namespace aurora
