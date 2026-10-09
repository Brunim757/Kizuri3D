#pragma once
#include <string>
namespace Kizuri {
enum class Severity {
  Trace = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
  Fatal = 4
};
using SeveritySink = void (*)(Severity level, const char* file, int line, const char* message);
void SetLogSink(SeveritySink sink);
void WriteLog(Severity level, const char* file, int line, const char* message);
void WriteLog(Severity level, const char* file, int line, const std::string& message);
const char* SeverityName(Severity level);
}
#define KZ_LOG_TRACE(msg) Kizuri::WriteLog(Kizuri::Severity::Trace, __FILE__, __LINE__, msg)
#define KZ_LOG_INFO(msg) Kizuri::WriteLog(Kizuri::Severity::Info, __FILE__, __LINE__, msg)
#define KZ_LOG_WARN(msg) Kizuri::WriteLog(Kizuri::Severity::Warning, __FILE__, __LINE__, msg)
#define KZ_LOG_ERROR(msg) Kizuri::WriteLog(Kizuri::Severity::Error, __FILE__, __LINE__, msg)
#define KZ_LOG_FATAL(msg) Kizuri::WriteLog(Kizuri::Severity::Fatal, __FILE__, __LINE__, msg)
