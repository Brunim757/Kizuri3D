#include "EditorApp.h"
#include "Kizuri/HardwareSpec.h"
#include <cstdio>
#include <exception>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
int main(int argc, char** argv) {
  try {
  {
    const Kizuri::HardwareSpec spec = Kizuri::DetectHardware();
    std::printf("Kizuri Editor | %s\n", spec.Summary().c_str());
    std::fflush(stdout);
  }
  Kizuri::EditorApp app;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-audio") {
      app.SetNoAudio(true);
    } else if (arg == "--no-assets") {
      app.SetNoAssets(true);
    } else if (arg == "--no-physics") {
      app.SetNoPhysics(true);
    }
  }
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
