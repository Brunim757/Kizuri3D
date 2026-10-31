#include <caligo/core/Assert.h>
#include <caligo/core/Log.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace caligo {

const char* ToString(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Fatal:
    return "FATAL";
  default:
    return "UNKNOWN";
  }
}

void ConsoleSink::Write(LogLevel, std::string_view line) {
  std::fwrite(line.data(), 1, line.size(), stdout);
  std::fflush(stdout);
#if defined(_WIN32)
  if (IsDebuggerPresent() != FALSE) {
    const std::string terminated(line);
    OutputDebugStringA(terminated.c_str());
  }
#endif
}

void ConsoleSink::Flush() {
  std::fflush(stdout);
}

FileSink::FileSink(std::filesystem::path path) : m_path(std::move(path)) {
  std::error_code error;
  if (m_path.has_parent_path()) {
    std::filesystem::create_directories(m_path.parent_path(), error);
  }
  m_file.open(m_path, std::ios::binary | std::ios::trunc);
}

FileSink::~FileSink() {
  Flush();
}

bool FileSink::IsOpen() const noexcept {
  return m_file.is_open();
}

void FileSink::Write(LogLevel, std::string_view line) {
  if (!m_file.is_open()) {
    return;
  }
  m_file.write(line.data(), static_cast<std::streamsize>(line.size()));
}

void FileSink::Flush() {
  if (m_file.is_open()) {
    m_file.flush();
  }
}

void Logger::AddSink(std::unique_ptr<LogSink> sink) {
  CALIGO_ASSERT_MSG(sink != nullptr, "log sink is null");
  if (!sink) {
    return;
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_sinks.push_back(std::move(sink));
}

void Logger::SetMinimumLevel(LogLevel level) noexcept {
  m_minimum.store(level, std::memory_order_relaxed);
}

bool Logger::IsEnabled(LogLevel level) const noexcept {
  return level >= m_minimum.load(std::memory_order_relaxed);
}

void Logger::Write(LogLevel level, std::string_view message) {
  if (!IsEnabled(level)) {
    return;
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &seconds);
#else
  localtime_r(&seconds, &local);
#endif
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  const size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
  const std::string line = std::format(
      "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}] [{}] [{:x}] {}\n", local.tm_year + 1900,
      local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec,
      milliseconds.count(), ToString(level), threadHash, message);

  std::lock_guard<std::mutex> lock(m_mutex);
  for (const std::unique_ptr<LogSink>& sink : m_sinks) {
    sink->Write(level, line);
  }
}

void Logger::Flush() {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (const std::unique_ptr<LogSink>& sink : m_sinks) {
    sink->Flush();
  }
}

Logger& GetLogger() {
  static Logger logger;
  return logger;
}

}
