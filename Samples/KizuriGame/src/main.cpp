#include "Kizuri/GameApp.h"
#include "Kizuri/HardwareSpec.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
namespace {
std::string FindFirstExisting(const char* const* candidates, int count) {
  for (int i = 0; i < count; ++i) {
    if (std::filesystem::exists(candidates[i])) {
      return std::filesystem::absolute(candidates[i]).string();
    }
  }
  return std::string();
}
}
int main(int argc, char** argv) {
  Kizuri::GameAppDesc desc;
  desc.windowTitle = "Kizuri Game";
  desc.width = 1280;
  desc.height = 720;
  const char* shaderDirs[4] = { "Shaders", "../Shaders", "../../Shaders", "build/bin/Release/Shaders" };
  const char* assetDirs[4] = { "Assets", "Samples/Assets", "../Assets", "../../Samples/Assets" };
  desc.shaderDir = FindFirstExisting(shaderDirs, 4);
  desc.assetsDir = FindFirstExisting(assetDirs, 4);
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-assets") {
      desc.loadAssets = false;
    } else if (arg == "--no-audio") {
      desc.enableAudio = false;
    } else if (arg == "--no-physics") {
      desc.enablePhysics = false;
    } else if (arg == "--lowspec") {
      desc.lowSpec = true;
      desc.renderScale = 0.5f;
    } else if (arg == "--scale" && i + 1 < argc) {
      desc.renderScale = static_cast<float>(atof(argv[++i]));
    } else if (arg.rfind("--", 0) != 0) {
      desc.scenePath = arg;
    }
  }
  const Kizuri::HardwareSpec spec = Kizuri::DetectHardware();
  std::printf("%s\n", spec.Summary().c_str());
  if (spec.lowSpec && !desc.lowSpec) {
    desc.lowSpec = true;
    desc.renderScale = 0.5f;
    std::printf("Hardware fraco: modo low-spec ativado (render scale 0.5, sem SSR/bloom). Use --no-assets/--no-audio/--no-physics para isolar.\n");
  }
  if (desc.shaderDir.empty()) {
    std::printf("Kizuri Game: pasta Shaders/ nao encontrada\n");
    return 1;
  }
  try {
    Kizuri::GameApp app;
    if (!app.Initialize(desc)) {
      std::printf("Kizuri Game: %s\n", app.LastError().c_str());
      std::printf("Pressione Enter para fechar...\n");
      (void)std::getchar();
      return 1;
    }
    const int code = app.Run();
    app.Shutdown();
    return code;
  } catch (const std::exception& e) {
    std::printf("Kizuri Game: excecao nao tratada: %s\n", e.what());
    std::printf("Pressione Enter para fechar...\n");
    (void)std::getchar();
    return 1;
  } catch (...) {
    std::printf("Kizuri Game: excecao desconhecida\n");
    std::printf("Pressione Enter para fechar...\n");
    (void)std::getchar();
    return 1;
  }
}
