#include "EditorApp.h"
#include "Kizuri/HardwareSpec.h"
#include <cstdio>
#include <exception>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
int main() {
  try {
  {
    const Kizuri::HardwareSpec spec = Kizuri::DetectHardware();
    std::printf("Kizuri Editor | %s\n", spec.Summary().c_str());
    std::fflush(stdout);
  }
  Kizuri::EditorApp app;
  if (!app.Initialize()) {
    std::printf("Editor init failed\n");
    std::string detail = app.LastInitError();
    if (detail.empty()) {
      detail = "Falha desconhecida.";
    }
    std::printf("%s\n", detail.c_str());
#ifdef _WIN32
    std::string msg = "Kizuri3D nao iniciou.\n" + detail + "\nVerifique DirectX 11 e os arquivos Shaders/ e Assets/ ao lado do executavel.";
    MessageBoxA(nullptr, msg.c_str(), "Kizuri3D Editor", MB_OK | MB_ICONERROR);
#endif
    return 1;
  }
  int code = app.Run();
  app.Shutdown();
  return code;
  } catch (const std::exception& e) {
    std::printf("Editor: excecao nao tratada: %s\n", e.what());
    std::printf("Pressione Enter para fechar...\n");
    (void)std::getchar();
    return 1;
  } catch (...) {
    std::printf("Editor: excecao desconhecida\n");
    std::printf("Pressione Enter para fechar...\n");
    (void)std::getchar();
    return 1;
  }
}
