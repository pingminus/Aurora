#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
namespace opengod::terminal {
struct Snapshot { std::string status, error, shell, bytes; uint64_t next = 0; unsigned long pid = 0; };
// Native launch options. The initial command is trusted, internal and ASCII (a
// known tool name); it is embedded into the shell command line before startup.
struct Options {
  std::string initial_command;  // Optional fixed command the shell runs on launch.
  bool test_cmd = false;        // Launch cmd.exe instead of PowerShell.
};
// UI methods only lock bounded queues. Worker state outlives asynchronous close.
class Session {
 public:
  explicit Session(const Options& options = {});
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  bool input(const std::string& bytes);
  bool resize(int columns, int rows);
  std::optional<Snapshot> read(uint64_t acknowledged);
  Snapshot inspect() const;
  void close();
  bool finished() const;
 private:
  struct State;
  std::shared_ptr<State> state_;
  static void run(std::shared_ptr<State>, const Options& options);
};
}
