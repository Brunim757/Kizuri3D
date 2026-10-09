#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Logger.h"
#include <windows.h>
#include <cstdio>
namespace Kizuri {
namespace {
SeveritySink g_sevSink = nullptr;
void DefaultSink(Severity level, const char* file, int line, const char* message) {
  const char* name = SeverityName(level);
  const char* shortFile = file;
  for (const char* p = file; *p != '\0'; ++p) {
    if (*p == '/' || *p == '\\') {
      shortFile = p + 1;
    }
  }
  char head[64];
  _snprintf_s(head, sizeof(head), _TRUNCATE, "[%s] %s(%d): ", name, shortFile, line);
  OutputDebugStringA(head);
  OutputDebugStringA(message);
  OutputDebugStringA("\n");
  if (level == Severity::Fatal) {
    std::fputs(head, stderr);
    std::fputs(message, stderr);
    std::fputs("\n", stderr);
  }
}
}
void SetLogSink(SeveritySink sink) {
  g_sevSink = sink;
}
void WriteLog(Severity level, const char* file, int line, const char* message) {
  if (message == nullptr) {
    message = "";
  }
  if (file == nullptr) {
    file = "";
  }
  if (g_sevSink != nullptr) {
    g_sevSink(level, file, line, message);
    return;
  }
  DefaultSink(level, file, line, message);
}
void WriteLog(Severity level, const char* file, int line, const std::string& message) {
  WriteLog(level, file, line, message.c_str());
}
const char* SeverityName(Severity level) {
  switch (level) {
    case Severity::Trace:
      return "TRACE";
    case Severity::Info:
      return "INFO";
    case Severity::Warning:
      return "WARN";
    case Severity::Error:
      return "ERROR";
    case Severity::Fatal:
      return "FATAL";
    default:
      return "?";
  }
}
}
