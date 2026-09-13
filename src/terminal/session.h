#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
namespace aurora::terminal {
struct Snapshot { std::string status, error, shell, bytes; uint64_t next = 0; unsigned long pid = 0; };
// UI methods only lock bounded queues. Worker state outlives asynchronous close.
class Session {
 public:
  explicit Session(bool test_cmd = false);
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
  static void run(std::shared_ptr<State>, bool test_cmd);
};
}
