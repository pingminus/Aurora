#pragma once

#include <string>
#include <string_view>

namespace aurora {

struct NavigationTarget {
  bool allowed = false;
  std::string url;
  std::string error;
  bool is_search = false;
};

// Omnibox admission policy, not a replacement for Chromium URL parsing.
// Call only for user-entered top-level navigation, never for subresources.
[[nodiscard]] NavigationTarget resolve_navigation(std::string_view input);

}  // namespace aurora
