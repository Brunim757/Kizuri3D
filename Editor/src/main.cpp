#include "EditorApp.h"
#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
int main() {
  Kizuri::EditorApp app;
  if (!app.Initialize()) {
    std::printf("Editor init failed\n");
#ifdef _WIN32
    MessageBoxA(nullptr, "Kizuri3D nao iniciou.\nVerifique DirectX 11 e os arquivos Shaders/ e Assets/ ao lado do executavel.", "Kizuri3D Editor", MB_OK | MB_ICONERROR);
#endif
    return 1;
  }
  int code = app.Run();
  app.Shutdown();
  return code;
}
