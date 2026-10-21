#pragma once
#include "Kizuri/GameLoop.h"
#include "Kizuri/GameWorld.h"
#include "Kizuri/RHI.h"
#include "Kizuri/Window.h"
#include <string>
namespace Kizuri {
struct GameAppDesc {
  std::string windowTitle = "Kizuri Game";
  int width = 1280;
  int height = 720;
  std::string scenePath;
  std::string assetsDir;
  std::string shaderDir;
  bool vsync = true;
  bool vsyncFixedStep = true;
  bool loadAssets = true;
  bool enablePhysics = true;
  bool enableAudio = true;
  bool lowSpec = false;
  float renderScale = 1.0f;
  float fixedStep = 1.0f / 60.0f;
  int maxStepsPerFrame = 4;
};
class GameApp {
public:
  GameApp();
  ~GameApp();
  bool Initialize(const GameAppDesc& desc);
  int Run();
  void Shutdown();
  void RequestQuit() { loop.RequestQuit(); }
  GameLoop& Loop() { return loop; }
  GameWorld& World() { return world; }
  const std::string& LastError() const { return lastError; }
  bool IsRunning() const { return running; }
private:
  void UpdateInput();
  void HandleResize();
  GameAppDesc desc;
  Window window;
  IRHI* rhi = nullptr;
  GameWorld world;
  GameLoop loop;
  std::string lastError;
  bool running = false;
  bool initialized = false;
  float camYaw = 0.0f;
  float camPitch = 0.15f;
};
}