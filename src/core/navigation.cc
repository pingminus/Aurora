#include "core/navigation.h"

#include <algorithm>
#include <cctype>

namespace aurora {
namespace {
bool ascii_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
std::string lowercase(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}
std::string encode_query(std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}
NavigationTarget deny(std::string error) {
  return {false, {}, std::move(error), false};
}
}  // namespace

NavigationTarget resolve_navigation(std::string_view input) {
  if (input.size() > 16384)
    return deny("Navigation input is too long.");
  while (!input.empty() && ascii_space(static_cast<unsigned char>(input.front())))
    input.remove_prefix(1);
  while (!input.empty() && ascii_space(static_cast<unsigned char>(input.back())))
    input.remove_suffix(1);
  if (input.empty())
    return {true, "aurora://newtab", {}, false};
  for (unsigned char c : input) {
    if (c < 0x20 || c == 0x7f || c == '\\')
      return deny("Navigation contains forbidden characters.");
  }
  const auto lower = lowercase(input);
  if (lower == "aurora://newtab" || lower == "aurora://newtab/" ||
      lower == "aurora://newtab/newtab.html")
    return {true, "aurora://newtab/", {}, false};
  if (lower == "about:blank")
    return {true, lower, {}, false};

  const bool http = lower.starts_with("http://");
  const bool https = lower.starts_with("https://");
  const auto authority_end = input.find_first_of("/?#");
  const auto candidate = input.substr(0, authority_end);
  const auto colon = candidate.find(':');
  const bool local =
      candidate == "localhost" || candidate.starts_with("localhost:") || candidate.starts_with("[");
  const bool dotted = candidate.find('.') != std::string_view::npos;
  const bool port =
      colon != std::string_view::npos && colon + 1 < candidate.size() &&
      std::all_of(candidate.begin() + static_cast<std::ptrdiff_t>(colon + 1), candidate.end(),
                  [](unsigned char c) { return c >= '0' && c <= '9'; });
  const auto first_colon = input.find(':');
  const auto first_separator = input.find_first_of("/?# ");
  const bool explicit_scheme =
      first_colon != std::string_view::npos &&
      (first_separator == std::string_view::npos || first_colon < first_separator);
  if (!http && !https && explicit_scheme && !(local || (dotted && port))) {
    return deny("This URL scheme is not permitted.");
  }
  if (http || https || ((local || dotted) && input.find(' ') == std::string_view::npos)) {
    std::string url = http || https ? std::string(input) : "https://" + std::string(input);
    const auto start = url.find("://") + 3;
    const auto end = url.find_first_of("/?#", start);
    const auto authority =
        std::string_view(url).substr(start, end == std::string::npos ? end : end - start);
    if (authority.empty() || authority.find('@') != std::string_view::npos ||
        authority.find('%') != std::string_view::npos ||
        authority.find(' ') != std::string_view::npos) {
      return deny("A valid host without embedded credentials is required.");
    }
    url.replace(0, start, http ? "http://" : "https://");
    return {true, std::move(url), {}, false};
  }
  return {true, "https://duckduckgo.com/?q=" + encode_query(input), {}, true};
}
}  // namespace aurora
