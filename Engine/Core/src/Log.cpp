#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Log.h"
#include <windows.h>
#include <cstdio>
namespace Kizuri {
namespace {
LogSink g_sink = nullptr;
void DefaultSink(LogLevel level, const char* file, int line, const char* message) {
  const char* name = LogLevelName(level);
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
  if (level == LogLevel::Fatal) {
    std::fputs(head, stderr);
    std::fputs(message, stderr);
    std::fputs("\n", stderr);
  }
}
}
void LogSetSink(LogSink sink) {
  g_sink = sink;
}
void LogWrite(LogLevel level, const char* file, int line, const char* message) {
  if (message == nullptr) {
    message = "";
  }
  if (file == nullptr) {
    file = "";
  }
  if (g_sink != nullptr) {
    g_sink(level, file, line, message);
    return;
  }
  DefaultSink(level, file, line, message);
}
void LogWrite(LogLevel level, const char* file, int line, const std::string& message) {
  LogWrite(level, file, line, message.c_str());
}
const char* LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return "TRACE";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warning:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
    case LogLevel::Fatal:
      return "FATAL";
    default:
      return "?";
  }
}
}
