#include "Kizuri/GameApp.h"
#include "Kizuri/Path.h"
#include <cstdio>
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
  if (argc > 1) {
    desc.scenePath = argv[1];
  }
  if (desc.shaderDir.empty()) {
    std::printf("Kizuri Game: pasta Shaders/ nao encontrada\n");
    return 1;
  }
  Kizuri::GameApp app;
  if (!app.Initialize(desc)) {
    std::printf("Kizuri Game: %s\n", app.LastError().c_str());
    return 1;
  }
  const int code = app.Run();
  app.Shutdown();
  return code;
}
