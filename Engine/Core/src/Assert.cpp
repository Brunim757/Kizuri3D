#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Assert.h"
#include "Kizuri/Log.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
namespace Kizuri {
void AssertFail(const char* expr, const char* file, int line) {
  AssertFailMessage(expr, "", file, line);
}
void AssertFailMessage(const char* expr, const char* message, const char* file, int line) {
  if (expr == nullptr) {
    expr = "?";
  }
  if (message == nullptr) {
    message = "";
  }
  if (file == nullptr) {
    file = "";
  }
  char buf[1024];
  if (message[0] != '\0') {
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "KZ_ASSERT failed: %s\n%s\n%s(%d)", expr, message, file, line);
  } else {
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "KZ_ASSERT failed: %s\n%s(%d)", expr, file, line);
  }
  LogWrite(LogLevel::Fatal, file, line, buf);
  MessageBoxA(nullptr, buf, "Kizuri3D Assertion", MB_OK | MB_ICONERROR);
  __debugbreak();
  std::abort();
}
}
