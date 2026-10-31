#include <caligo/platform/System.h>
#include <windows.h>

namespace caligo::platform {

void ConfigureProcess() noexcept {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

}
