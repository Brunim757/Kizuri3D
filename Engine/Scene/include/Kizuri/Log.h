#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <mutex>
namespace Kizuri {
enum class LogLevel {
  Info,
  Success,
  Warning,
  Error
};
struct LogEntry {
  LogLevel level;
  std::string text;
  uint64_t seq;
};
class LogStore {
public:
  LogStore();
  void Add(LogLevel level, const std::string& text);
  size_t Count() const;
  const LogEntry& At(size_t i) const;
  size_t CountLevel(LogLevel level) const;
  void Clear();
private:
  std::vector<LogEntry> entries;
  uint64_t nextSeq;
  mutable std::mutex mtx;
};
}
