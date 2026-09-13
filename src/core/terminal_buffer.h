#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace aurora::terminal {
constexpr size_t output_limit = 256 * 1024;
constexpr size_t input_limit = 64 * 1024;
inline bool valid_size(int columns,int rows) { return columns>=2&&columns<=400&&rows>=1&&rows<=200; }
// Raw UTF-8/VT bytes remain intact across transport chunks; xterm owns decoding.
class OutputBuffer {
 public:
  bool append(const std::string& bytes) { if(bytes.size()>output_limit-data_.size())return false;data_+=bytes;return true; }
  std::optional<std::string> read(uint64_t ack) {
    if(ack<base_||ack>delivered_)return {};
    data_.erase(0,static_cast<size_t>(ack-base_));base_=ack;
    auto chunk=data_.substr(0,32768);delivered_=base_+chunk.size();return chunk;
  }
  uint64_t next() const { return delivered_; }
  size_t size() const { return data_.size(); }
 private:
  std::string data_; uint64_t base_=0,delivered_=0;
};
}
