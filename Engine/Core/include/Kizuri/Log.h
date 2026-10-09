#pragma once
#include <string>
namespace Kizuri {
enum class LogLevel {
  Trace = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
  Fatal = 4
};
using LogSink = void (*)(LogLevel level, const char* file, int line, const char* message);
void LogSetSink(LogSink sink);
void LogWrite(LogLevel level, const char* file, int line, const char* message);
void LogWrite(LogLevel level, const char* file, int line, const std::string& message);
const char* LogLevelName(LogLevel level);
}
#define KZ_LOG_TRACE(msg) Kizuri::LogWrite(Kizuri::LogLevel::Trace, __FILE__, __LINE__, msg)
#define KZ_LOG_INFO(msg) Kizuri::LogWrite(Kizuri::LogLevel::Info, __FILE__, __LINE__, msg)
#define KZ_LOG_WARN(msg) Kizuri::LogWrite(Kizuri::LogLevel::Warning, __FILE__, __LINE__, msg)
#define KZ_LOG_ERROR(msg) Kizuri::LogWrite(Kizuri::LogLevel::Error, __FILE__, __LINE__, msg)
#define KZ_LOG_FATAL(msg) Kizuri::LogWrite(Kizuri::LogLevel::Fatal, __FILE__, __LINE__, msg)
