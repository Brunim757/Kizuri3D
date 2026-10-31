#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace caligo {

enum class LogLevel : uint8_t { Trace, Debug, Info, Warn, Error, Fatal };

[[nodiscard]] const char* ToString(LogLevel level) noexcept;

class LogSink {
public:
  virtual ~LogSink() = default;
  virtual void Write(LogLevel level, std::string_view line) = 0;
  virtual void Flush() = 0;
};

class ConsoleSink final : public LogSink {
public:
  void Write(LogLevel level, std::string_view line) override;
  void Flush() override;
};

class FileSink final : public LogSink {
public:
  explicit FileSink(std::filesystem::path path);
  ~FileSink() override;

  [[nodiscard]] bool IsOpen() const noexcept;

  void Write(LogLevel level, std::string_view line) override;
  void Flush() override;

private:
  std::filesystem::path m_path;
  std::ofstream m_file;
};

class Logger final {
public:
  Logger() = default;

  void AddSink(std::unique_ptr<LogSink> sink);
  void SetMinimumLevel(LogLevel level) noexcept;
  [[nodiscard]] bool IsEnabled(LogLevel level) const noexcept;
  void Write(LogLevel level, std::string_view message);
  void Flush();

private:
  std::atomic<LogLevel> m_minimum{LogLevel::Info};
  std::mutex m_mutex;
  std::vector<std::unique_ptr<LogSink>> m_sinks;
};

[[nodiscard]] Logger& GetLogger();

template <typename... Args>
void Log(LogLevel level, std::format_string<Args...> format, Args&&... args) {
  Logger& logger = GetLogger();
  if (!logger.IsEnabled(level)) {
    return;
  }
  logger.Write(level, std::format(format, std::forward<Args>(args)...));
}

#define CALIGO_LOG_TRACE(...) ::caligo::Log(::caligo::LogLevel::Trace, __VA_ARGS__)
#define CALIGO_LOG_DEBUG(...) ::caligo::Log(::caligo::LogLevel::Debug, __VA_ARGS__)
#define CALIGO_LOG_INFO(...) ::caligo::Log(::caligo::LogLevel::Info, __VA_ARGS__)
#define CALIGO_LOG_WARN(...) ::caligo::Log(::caligo::LogLevel::Warn, __VA_ARGS__)
#define CALIGO_LOG_ERROR(...) ::caligo::Log(::caligo::LogLevel::Error, __VA_ARGS__)
#define CALIGO_LOG_FATAL(...) ::caligo::Log(::caligo::LogLevel::Fatal, __VA_ARGS__)

}
