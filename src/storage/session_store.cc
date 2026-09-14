#include "storage/session_store.h"

#include "core/navigation.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace opengod::storage {
namespace {
constexpr std::size_t kMaxFileSize = 4 * 1024 * 1024;
constexpr std::size_t kMaxTabs = 256;
constexpr std::size_t kMaxUrlSize = 16384;
constexpr std::size_t kMaxTitleSize = 4096;
constexpr std::string_view kMagic("AURSESS\0", 8);
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kHeaderSize = 24;

void append_u32(std::string& data, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    data.push_back(static_cast<char>((value >> shift) & 255));
}

std::uint32_t crc32(std::string_view data) {
  std::uint32_t crc = 0xffffffff;
  for (unsigned char byte : data) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

class Reader {
 public:
  explicit Reader(std::string_view data) : data_(data) {}
  bool u32(std::uint32_t& value) {
    if (data_.size() < 4) return false;
    value = 0;
    for (int i = 0; i < 4; ++i)
      value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data_[i])) << (8 * i);
    data_.remove_prefix(4);
    return true;
  }
  bool string(std::string& result, std::size_t limit) {
    std::uint32_t length = 0;
    if (!u32(length) || length > limit || length > data_.size()) return false;
    result.assign(data_.substr(0, length));
    data_.remove_prefix(length);
    return true;
  }
  bool empty() const { return data_.empty(); }

 private:
  std::string_view data_;
};

std::optional<std::string> validate(const Session& session) {
  if (session.tabs.size() > kMaxTabs) return "Session exceeds 256 tabs.";
  if ((session.tabs.empty() && session.active_index != 0) ||
      (!session.tabs.empty() && session.active_index >= session.tabs.size()))
    return "Session has an invalid active tab index.";
  for (const auto& tab : session.tabs) {
    if (tab.url.empty() || tab.url.size() > kMaxUrlSize || tab.title.size() > kMaxTitleSize)
      return "Session tab exceeds the URL or title size limit.";
    const auto target = resolve_navigation(tab.url);
    if (!target.allowed || target.is_search)
      return "Session contains a navigation URL disallowed by policy.";
  }
  return std::nullopt;
}

std::string encode(const Session& session) {
  std::string payload;
  for (const auto& tab : session.tabs) {
    append_u32(payload, static_cast<std::uint32_t>(tab.url.size()));
    payload += tab.url;
    append_u32(payload, static_cast<std::uint32_t>(tab.title.size()));
    payload += tab.title;
    append_u32(payload, (tab.pinned ? 1u : 0u) | (tab.muted ? 2u : 0u));
  }
  std::string result(kMagic);
  append_u32(result, kVersion);
  append_u32(result, static_cast<std::uint32_t>(session.tabs.size()));
  append_u32(result, static_cast<std::uint32_t>(session.active_index));
  // Checksum includes version/count/index as well as the payload.
  append_u32(result, crc32(std::string_view(result).substr(8) + payload));
  result += payload;
  return result;
}

LoadResult decode(const std::string& bytes) {
  auto failure = [](const char* reason) { return LoadResult{std::nullopt, reason, false}; };
  if (bytes.size() < kHeaderSize || bytes.size() > kMaxFileSize ||
      std::string_view(bytes).substr(0, 8) != kMagic)
    return failure("Session file has an invalid header or size.");
  Reader header(std::string_view(bytes).substr(8, 16));
  std::uint32_t version = 0, count = 0, active = 0, checksum = 0;
  if (!header.u32(version) || !header.u32(count) || !header.u32(active) || !header.u32(checksum))
    return failure("Session header is truncated.");
  if (version != kVersion) return failure("Session format version is unsupported.");
  if (count > kMaxTabs || (count == 0 ? active != 0 : active >= count))
    return failure("Session tab count or active index is invalid.");
  const auto payload = std::string_view(bytes).substr(kHeaderSize);
  if (crc32(bytes.substr(8, 12) + std::string(payload)) != checksum)
    return failure("Session checksum does not match.");
  Reader reader(payload);
  Session session;
  session.active_index = active;
  session.tabs.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    SessionTab tab;
    std::uint32_t flags = 0;
    if (!reader.string(tab.url, kMaxUrlSize) || !reader.string(tab.title, kMaxTitleSize) ||
        !reader.u32(flags) || flags > 3)
      return failure("Session tab record is truncated or invalid.");
    tab.pinned = (flags & 1) != 0;
    tab.muted = (flags & 2) != 0;
    session.tabs.push_back(std::move(tab));
  }
  if (!reader.empty()) return failure("Session contains unexpected trailing data.");
  if (auto error = validate(session)) return {std::nullopt, *error, false};
  return {std::move(session), {}, false};
}

LoadResult read_file(const std::filesystem::path& path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) return {std::nullopt, "Could not inspect the session file: " + error.message(), false};
  if (!exists) return {};
  const auto size = std::filesystem::file_size(path, error);
  if (error) return {std::nullopt, "Could not read session file size: " + error.message(), false};
  if (size > kMaxFileSize) return {std::nullopt, "Session file exceeds 4 MiB.", false};
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {std::nullopt, "Could not open the session file.", false};
  std::string bytes(static_cast<std::size_t>(size), '\0');
  if (!stream.read(bytes.data(), static_cast<std::streamsize>(size)))
    return {std::nullopt, "Could not read the complete session file.", false};
  if (stream.peek() != std::char_traits<char>::eof())
    return {std::nullopt, "Session file changed while being read.", false};
  return decode(bytes);
}

std::string os_error(const char* action, int code) {
  return std::string(action) + ": " + std::system_category().message(code);
}

std::optional<std::string> atomic_write(const std::filesystem::path& destination,
                                      const std::string& bytes) {
  static std::atomic<std::uint64_t> sequence{0};
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  auto temporary = destination;
  temporary += ".tmp." + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1));
  struct TemporaryCleanup {
    std::filesystem::path path;
    ~TemporaryCleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); }
  };
#ifdef _WIN32
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return os_error("Could not create temporary session file", static_cast<int>(GetLastError()));
  TemporaryCleanup cleanup{temporary};
  DWORD written = 0;
  if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
      written != bytes.size()) {
    const auto error = GetLastError();
    CloseHandle(file);
    return os_error("Could not write complete session file", static_cast<int>(error));
  }
  if (!FlushFileBuffers(file)) {
    const auto error = GetLastError();
    CloseHandle(file);
    return os_error("Could not flush session file", static_cast<int>(error));
  }
  if (!CloseHandle(file))
    return os_error("Could not close session file", static_cast<int>(GetLastError()));
  if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return os_error("Could not atomically replace session file", static_cast<int>(GetLastError()));
#else
  const int file = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (file < 0) return os_error("Could not create temporary session file", errno);
  TemporaryCleanup cleanup{temporary};
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto written = ::write(file, bytes.data() + offset, bytes.size() - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      const int error = errno;
      ::close(file);
      return os_error("Could not write complete session file", error);
    }
    offset += static_cast<std::size_t>(written);
  }
  if (::fsync(file) != 0) {
    const int error = errno;
    ::close(file);
    return os_error("Could not flush session file", error);
  }
  if (::close(file) != 0) return os_error("Could not close session file", errno);
  std::error_code error;
  std::filesystem::rename(temporary, destination, error);
  if (error) return "Could not atomically replace session file: " + error.message();
  const int directory = ::open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  if (directory < 0) return os_error("Could not open session directory for flushing", errno);
  if (::fsync(directory) != 0) {
    const int code = errno;
    ::close(directory);
    return os_error("Could not flush session directory", code);
  }
  if (::close(directory) != 0) return os_error("Could not close session directory", errno);
#endif
  return std::nullopt;
}
}  // namespace

SessionStore::SessionStore(std::filesystem::path profile_directory)
    : profile_directory_(std::move(profile_directory)) {}

LoadResult SessionStore::load() const {
  const auto primary = read_file(profile_directory_ / "session.dat");
  if (primary.snapshot) return primary;
  const auto backup = read_file(profile_directory_ / "session.dat.bak");
  if (backup.snapshot)
    return {backup.snapshot, primary.error.empty() ? "Primary session is missing; recovered backup."
                                                 : primary.error + " Recovered backup.", true};
  if (primary.error.empty() && backup.error.empty()) return {};
  return {std::nullopt, primary.error.empty() ? "Primary session is missing. " + backup.error
                                            : primary.error + (backup.error.empty() ? " No backup available." : " " + backup.error), false};
}

std::optional<std::string> SessionStore::save(const Session& session) const {
  if (auto error = validate(session)) return error;
  const auto bytes = encode(session);
  if (bytes.size() > kMaxFileSize) return "Session file exceeds 4 MiB.";
  std::error_code error;
  std::filesystem::create_directories(profile_directory_, error);
  if (error) return "Could not create session directory: " + error.message();
  const auto primary_path = profile_directory_ / "session.dat";
  const auto previous = read_file(primary_path);
  // Never replace a known-good backup with a corrupt primary after recovery.
  if (previous.snapshot) {
    if (auto failure = atomic_write(profile_directory_ / "session.dat.bak", encode(*previous.snapshot)))
      return "Could not preserve session backup. " + *failure;
  }
  return atomic_write(primary_path, bytes);
}
}  // namespace opengod::storage
