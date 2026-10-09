#include "EditorApp.h"
#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
int main() {
  std::remove("kizuri_startup.log");
  Kizuri::EditorApp app;
  if (!app.Initialize()) {
    std::string tail = "Editor init failed";
    FILE* fp = std::fopen("kizuri_startup.log", "rb");
    if (fp != nullptr) {
      std::fseek(fp, 0, SEEK_END);
      long sz = std::ftell(fp);
      long start = sz > 400 ? sz - 400 : 0;
      std::fseek(fp, start, SEEK_SET);
      char buf[512] = { 0 };
      size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
      std::fclose(fp);
      if (n > 0) {
        tail = std::string("Kizuri3D nao iniciou.\nDetalhe (kizuri_startup.log):\n") + buf;
      }
    }
    std::printf("%s\n", tail.c_str());
#ifdef _WIN32
    MessageBoxA(nullptr, tail.c_str(), "Kizuri3D Editor", MB_OK | MB_ICONERROR);
#endif
    return 1;
  }
  int code = app.Run();
  app.Shutdown();
  return code;
}
