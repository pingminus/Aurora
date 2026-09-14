#include "core/browser_state.h"
#include "core/navigation.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char* expression, int line) {
  if (!condition) {
    ++failures;
    std::cerr << "Line " << line << ": " << expression << '\n';
  }
}
#define CHECK(value) check(static_cast<bool>(value), #value, __LINE__)

void navigation_policy() {
  using opengod::resolve_navigation;
  CHECK(resolve_navigation("").url == "opengod://newtab");
  CHECK(resolve_navigation("  example.com/path  ").url == "https://example.com/path");
  CHECK(resolve_navigation("HTTP://example.com").url == "http://example.com");
  CHECK(resolve_navigation("localhost:8080").url == "https://localhost:8080");
  CHECK(resolve_navigation("example.com:8443/path").allowed);
  CHECK(resolve_navigation("[::1]:8080").allowed);
  CHECK(resolve_navigation("about:blank").allowed);
  CHECK(!resolve_navigation("opengod://settings").allowed);
  CHECK(resolve_navigation("opengod://newtab/").allowed);
  CHECK(resolve_navigation("opengod://newtab/newtab.html").allowed);
  CHECK(!resolve_navigation("opengod://newtab/../shell").allowed);
  const auto search = resolve_navigation("C++ browser & tabs");
  CHECK(search.allowed && search.is_search);
  CHECK(search.url == "https://duckduckgo.com/?q=C%2B%2B%20browser%20%26%20tabs");
  for (const auto* url :
       {"javascript:alert(1)", "JaVaScRiPt:alert(1)", "data:text/html,hi", "file:///etc/passwd",
        "chrome://flags", "opengod://unknown", "https://user:pass@example.com", "https://",
        "https://exa mple.com", "https://%65xample.com", "https:\\evil.test",
        "https://good.test\n.evil.test"}) {
    CHECK(!resolve_navigation(url).allowed);
  }
  CHECK(!resolve_navigation(std::string("https://a.test\0.evil", 20)).allowed);
  CHECK(!resolve_navigation(std::string(16385, 'a')).allowed);
}

void lifecycle() {
  opengod::BrowserState state;
  CHECK(state.tabs().empty() && state.active_tab() == 0);
  CHECK(state.reopen_closed_tab() == 0);
  const auto first = state.create_tab("example.com");
  const auto second = state.create_tab();
  CHECK(first != 0 && second > first && state.active_tab() == second);
  CHECK(state.navigate(first, "a search"));
  CHECK(state.tabs().front().url == "https://duckduckgo.com/?q=a%20search");
  const auto original_url = state.tabs().front().url;
  CHECK(!state.navigate(first, "javascript:alert(1)"));
  CHECK(state.tabs().front().url == original_url);
  CHECK(!state.last_error().empty());
  CHECK(state.create_tab("file:///secret") == 0 && state.tabs().size() == 2);
  CHECK(state.set_pinned(first, true) && state.set_muted(first, true));
  CHECK(state.set_title(first, "Article"));
  const auto copy = state.duplicate_tab(first);
  CHECK(copy > second && state.active_tab() == copy);
  CHECK(!state.tabs().back().pinned && !state.tabs().back().muted);
  CHECK(state.tabs().back().volume == 100); // Duplicates have a new audio identity.
  CHECK(state.tabs().back().title == "Article");
  CHECK(state.close_tab(first));
  CHECK(state.active_tab() == copy);
  const auto restored = state.reopen_closed_tab();
  CHECK(restored > copy && state.active_tab() == restored);
  CHECK(state.tabs().back().pinned && state.tabs().back().muted);
  CHECK(!state.activate_tab(first) && !state.close_tab(first));
  CHECK(!state.navigate(first, "example.org") && state.duplicate_tab(first) == 0);
  CHECK(!state.set_pinned(first, true) && !state.set_muted(first, true));
  while (!state.tabs().empty())
    CHECK(state.close_tab(state.tabs().back().id));
  CHECK(state.active_tab() == 0);
}

void workspaces() {
  opengod::BrowserState state;
  const auto first = state.create_tab();
  const auto work = state.create_workspace("Work");
  CHECK(work > 1 && state.create_workspace("") == 0);
  CHECK(!state.activate_workspace(999));
  CHECK(state.activate_workspace(work) && state.active_tab() == 0);
  const auto second = state.create_tab();
  CHECK(state.tabs().back().workspace_id == work);
  CHECK(state.activate_tab(first) && state.active_workspace() == 1);
  CHECK(state.move_to_workspace(first, work) && state.active_tab() == 0);
  CHECK(!state.move_to_workspace(first, 999));
  CHECK(state.activate_tab(second) && state.active_workspace() == work);
  CHECK(state.close_tab(second) && state.active_tab() == first);
  CHECK(state.close_tab(first) && state.active_tab() == 0);
  CHECK(state.activate_workspace(1));
  CHECK(state.reopen_closed_tab() != 0 && state.active_workspace() == work);
}

void closed_tab_limit() {
  opengod::BrowserState state;
  for (int i = 0; i < 40; ++i)
    CHECK(state.close_tab(state.create_tab()));
  int count = 0;
  while (state.reopen_closed_tab() != 0)
    ++count;
  CHECK(count == 25);
}

void bookmarks() {
  opengod::BrowserState state;
  const auto tab = state.create_tab("https://example.com");
  CHECK(state.set_title(tab, "Example"));
  const auto saved = state.add_bookmark(tab);
  CHECK(saved != 0 && state.bookmarks().size() == 1);
  CHECK(state.bookmarks().front().id == saved);
  CHECK(state.bookmarks().front().title == "Example");
  CHECK(state.bookmarks().front().url == "https://example.com");
  // Adding the same URL again is idempotent.
  CHECK(state.add_bookmark(tab) == saved && state.bookmarks().size() == 1);
  CHECK(state.add_bookmark(0) == 0 && state.bookmarks().size() == 1);
  const auto other = state.create_tab("https://example.org");
  CHECK(state.set_title(other, "Other"));
  const auto second = state.add_bookmark(other);
  CHECK(second > saved && state.bookmarks().size() == 2);
  CHECK(state.remove_bookmark(saved));
  CHECK(state.bookmarks().size() == 1);
  CHECK(!state.remove_bookmark(saved) && !state.remove_bookmark(second + 99));
  // Bookmarks survive the tab that created them.
  CHECK(state.close_tab(tab) && state.bookmarks().size() == 1);
  CHECK(state.add_bookmark(tab) == 0);
  // Internal pages and terminals are not bookmarked.
  const auto newtab = state.create_tab("opengod://newtab");
  CHECK(state.add_bookmark(newtab) == 0 && state.add_bookmark(other + 999) == 0);
  CHECK(state.bookmarks().size() == 1);
}

void rename_bookmarks() {
  opengod::BrowserState state;
  const auto tab = state.create_tab("https://example.com");
  const auto id = state.add_bookmark(tab);
  CHECK(state.rename_bookmark(id, "Documentation"));
  CHECK(state.bookmarks().front().title == "Documentation");
  CHECK(state.bookmarks().front().id == id);
  CHECK(state.bookmarks().front().url == "https://example.com");
  CHECK(!state.rename_bookmark(id, ""));
  CHECK(!state.rename_bookmark(id, " \t\r\n"));
  CHECK(!state.rename_bookmark(id, std::string(257, 'x')));
  CHECK(!state.rename_bookmark(0, "Missing"));
  CHECK(!state.rename_bookmark(id + 999, "Missing"));
  CHECK(state.bookmarks().front().title == "Documentation");
  CHECK(state.rename_bookmark(id, std::string(256, 'x')));
  std::string unicode;
  for (int i = 0; i < 128; ++i) unicode += "\xc3\xa9";
  CHECK(state.rename_bookmark(id, unicode));
  CHECK(!state.rename_bookmark(id, unicode + "x"));
  CHECK(state.bookmarks().front().title == unicode);
  CHECK(state.close_tab(tab));
  CHECK(state.rename_bookmark(id, "After closing tab"));
  CHECK(state.bookmarks().size() == 1);
  CHECK(state.remove_bookmark(id));
  CHECK(!state.rename_bookmark(id, "Removed"));
}

void randomized_state_invariants() {
  opengod::BrowserState state;
  const auto workspace = state.create_workspace("Work");
  std::mt19937 random(2026);
  for (int i = 0; i < 5000; ++i) {
    const auto id = state.tabs().empty() ? 0 : state.tabs()[random() % state.tabs().size()].id;
    switch (random() % 10) {
      case 0:
        (void)state.create_tab();
        break;
      case 1:
        state.close_tab(id);
        break;
      case 2:
        state.activate_tab(id);
        break;
      case 3:
        (void)state.duplicate_tab(id);
        break;
      case 4:
        (void)state.reopen_closed_tab();
        break;
      case 5:
        state.activate_workspace((random() % 2) ? 1 : workspace);
        break;
      case 6:
        state.move_to_workspace(id, (random() % 2) ? 1 : workspace);
        break;
      case 7:
        state.navigate(id, "example.com");
        break;
      case 8:
        (void)state.add_bookmark(id);
        break;
      case 9: {
        const auto& marks = state.bookmarks();
        if (!marks.empty())
          state.remove_bookmark(marks[random() % marks.size()].id);
        break;
      }
    }
    std::set<std::uint64_t> ids;
    bool active_found = false;
    bool workspace_has_tabs = false;
    for (const auto& tab : state.tabs()) {
      CHECK(tab.id != 0 && ids.insert(tab.id).second);
      CHECK(tab.workspace_id == 1 || tab.workspace_id == workspace);
      if (tab.workspace_id == state.active_workspace())
        workspace_has_tabs = true;
      if (tab.id == state.active_tab()) {
        active_found = true;
        CHECK(tab.workspace_id == state.active_workspace());
      }
    }
    CHECK(active_found == workspace_has_tabs);
    CHECK(active_found || state.active_tab() == 0);
    std::set<std::uint64_t> mark_ids;
    for (const auto& mark : state.bookmarks()) {
      CHECK(mark.id != 0 && mark_ids.insert(mark.id).second);
      CHECK(!mark.url.empty() && mark.url.starts_with("opengod://") == false);
    }
  }
}
}  // namespace

int main() {
  { opengod::BrowserState state;const auto id=state.create_terminal();CHECK(id!=0&&state.tabs().back().terminal);CHECK(!state.navigate(id,"https://example.com"));CHECK(state.duplicate_tab(id)==0);CHECK(state.close_tab(id));CHECK(state.reopen_closed_tab()==0);CHECK(state.create_tab("opengod://terminal/")==0); }

  {
    opengod::BrowserState audio;
    const auto a = audio.create_tab("https://example.com");
    const auto b = audio.create_tab("https://example.org");
    CHECK(audio.tabs()[0].volume == 100 && !audio.tabs()[0].muted);
    CHECK(audio.tabs()[1].volume == 100 && !audio.tabs()[1].muted);
    CHECK(audio.set_volume(a, 37));
    CHECK(audio.set_muted(a, true));
    CHECK(audio.tabs()[0].volume == 37 && audio.tabs()[0].muted);
    CHECK(audio.tabs()[1].volume == 100 && !audio.tabs()[1].muted);
    CHECK(audio.set_volume(a, 62));
    CHECK(audio.set_muted(a, false));
    CHECK(audio.tabs()[0].volume == 62 && !audio.tabs()[0].muted);
    CHECK(!audio.set_volume(a, -1) && !audio.set_volume(a, 101));
    CHECK(!audio.set_volume(a, 2147483647));
    CHECK(audio.tabs()[0].volume == 62);
    CHECK(audio.navigate(a, "https://example.net"));
    CHECK(audio.tabs()[0].volume == 62);
    CHECK(audio.set_volume(b, 0));
    CHECK(audio.tabs()[0].volume == 62 && audio.tabs()[1].volume == 0);
    const auto duplicate = audio.duplicate_tab(a);
    CHECK(duplicate != a && audio.tabs().back().volume == 100 && !audio.tabs().back().muted);
    CHECK(audio.close_tab(a));
    CHECK(!audio.set_volume(a, 50) && !audio.set_muted(a, true));
    const auto restored = audio.reopen_closed_tab();
    CHECK(restored != a && audio.tabs().back().volume == 62);
  }
  navigation_policy();
  lifecycle();
  workspaces();
  closed_tab_limit();
  bookmarks();
  rename_bookmarks();
  randomized_state_invariants();
  if (failures)
    std::cerr << failures << " checks failed\n";
  else
    std::cout << "Core tests passed, including 5000 state transitions.\n";
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
