#include "EditorApp.h"
#include "Kizuri/HardwareSpec.h"
#include "Kizuri/Core.h"
#include <cstdio>
#include <exception>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <exception>
#endif
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
  if (ep != nullptr && ep->ExceptionRecord != nullptr) {
    const EXCEPTION_RECORD* rec = ep->ExceptionRecord;
    std::printf("\n[CRASH] codigo 0x%08X  endereco %p\n", rec->ExceptionCode, rec->ExceptionAddress);
  } else {
    std::printf("\n[CRASH] excecao desconhecida\n");
  }
  std::fflush(stdout);
  std::printf("Pressione Enter para fechar...\n");
  (void)std::getchar();
  return EXCEPTION_EXECUTE_HANDLER;
}
int main(int argc, char** argv) {
  SetUnhandledExceptionFilter(&CrashHandler);
  try {
  {
    const Kizuri::HardwareSpec spec = Kizuri::DetectHardware();
    std::printf("Kizuri Editor build %s | %s\n", Kizuri::BuildId(), spec.Summary().c_str());
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
