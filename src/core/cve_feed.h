#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <set>
namespace opengod::cve {
using Time = std::chrono::sys_seconds;
std::optional<Time> parse_time(const std::string& value);
std::string format_time(Time value);
Time day_start(Time now);
bool valid_id(const std::string& id);
struct Metric { std::string version, source, type; double score; };
std::optional<Metric> select_metric(const std::vector<Metric>& metrics);
std::string severity(const Metric& metric);
struct Entry { std::string id, description, published, modified; std::optional<Metric> metric; };
void sort_entries(std::vector<Entry>& entries);
class Batch {
 public:
  Batch(Time start, Time end) : start_(start), end_(end) {}
  bool append(int offset, int total, int count, std::vector<Entry> entries);
  bool complete() const { return total_ >= 0 && next_ == total_; }
  int next() const { return next_; }
  const std::vector<Entry>& entries() const { return entries_; }
 private:
  Time start_, end_;
  int next_ = 0, total_ = -1;
  std::set<std::string> ids_;
  std::vector<Entry> entries_;
};
struct Cache {
  std::vector<Entry> entries;
  Time start{}, end{}, fetched{}, attempted{};
  bool available = false, loading = false;
  std::string error;
  bool begin(Time now, bool refresh);
  void success(const Batch& batch, Time now);
  void fail(std::string message) { loading = false; error = std::move(message); }
};
}
