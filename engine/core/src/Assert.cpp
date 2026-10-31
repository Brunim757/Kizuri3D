#include <caligo/core/Assert.h>
#include <caligo/core/Log.h>
#include <cstdlib>

namespace caligo {

void FailAssert(const char* expression, const char* file, int line, const char* message) {
  if (message != nullptr) {
    Log(LogLevel::Fatal, "Assertion failed: {} at {}:{} ({})", expression, file, line, message);
  } else {
    Log(LogLevel::Fatal, "Assertion failed: {} at {}:{}", expression, file, line);
  }
  GetLogger().Flush();
  std::abort();
}

}
