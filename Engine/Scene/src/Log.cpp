#include "Kizuri/Log.h"
namespace Kizuri {
LogStore::LogStore()
  : nextSeq(1) {
}
void LogStore::Add(LogLevel level, const std::string& text) {
  std::lock_guard<std::mutex> lock(mtx);
  LogEntry e;
  e.level = level;
  e.text = text;
  e.seq = nextSeq++;
  entries.push_back(e);
  if (entries.size() > 1000) {
    entries.erase(entries.begin());
  }
}
size_t LogStore::Count() const {
  std::lock_guard<std::mutex> lock(mtx);
  return entries.size();
}
const LogEntry& LogStore::At(size_t i) const {
  return entries[i];
}
size_t LogStore::CountLevel(LogLevel level) const {
  std::lock_guard<std::mutex> lock(mtx);
  size_t n = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].level == level) {
      ++n;
    }
  }
  return n;
}
void LogStore::Clear() {
  std::lock_guard<std::mutex> lock(mtx);
  entries.clear();
}
}
