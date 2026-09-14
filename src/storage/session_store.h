#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace opengod::storage {

struct SessionTab {
  std::string url;
  std::string title;
  bool pinned = false;
  bool muted = false;
};

struct Session {
  std::vector<SessionTab> tabs;
  std::size_t active_index = 0;
};

struct LoadResult {
  std::optional<Session> snapshot;
  // Empty for successful load and first run. On recovery this explains why the
  // backup was needed; without a snapshot it describes a load failure.
  std::string error;
  bool recovered = false;
};

// Own one store per profile, serialize calls, and run disk I/O off the UI thread.
// Format v1 is bounded, little-endian and checksummed. No web engine dependency.
class SessionStore {
 public:
  explicit SessionStore(std::filesystem::path profile_directory);
  [[nodiscard]] LoadResult load() const;
  // nullopt means success; any error leaves the prior primary intact unless the
  // operating system reported a failure after committing an atomic replacement.
  [[nodiscard]] std::optional<std::string> save(const Session& session) const;

 private:
  std::filesystem::path profile_directory_;
};

}  // namespace opengod::storage
