#include "Kizuri/GameApp.h"
#include "Kizuri/Clock.h"
#include "Kizuri/Input.h"
#include "Kizuri/Logger.h"
#include <cmath>
#include <cstdio>
#include <vector>
namespace Kizuri {
GameApp::GameApp() {
}
GameApp::~GameApp() {
  Shutdown();
}
bool GameApp::Initialize(const GameAppDesc& inDesc) {
  if (initialized) {
    return true;
  }
  desc = inDesc;
  if (!window.Create(desc.windowTitle.size() > 0 ? desc.windowTitle.c_str() : L"Kizuri Game", desc.width, desc.height)) {
    lastError = "Falha ao criar a janela";
    return false;
  }
  RawInputPoll::Initialize(window.NativeHandle());
  RHIDesc rdesc;
  rdesc.windowHandle = window.NativeHandle();
  rdesc.width = window.Width();
  rdesc.height = window.Height();
  rdesc.vsync = desc.vsync;
  rhi = CreateRHI(RHI_API::D3D11);
  if (rhi == nullptr || !rhi->Initialize(rdesc)) {
    lastError = "DirectX 11 nao inicializado (device, driver ou Feature Level).";
    if (rhi != nullptr) {
      const char* reason = rhi->LastError();
      if (reason != nullptr && reason[0] != '\0') {
        lastError += " Detalhe: ";
        lastError += reason;
      }
    }
    return false;
  }
  if (!world.Initialize(rhi, window.Width(), window.Height(), desc.shaderDir, desc.assetsDir)) {
    lastError = world.LastError();
    return false;
  }
  if (!desc.scenePath.empty() && world.LoadSceneFile(desc.scenePath)) {
    Log(LogLevel::Info, "Cena carregada: %s", desc.scenePath.c_str());
  } else {
    world.CreateFallbackScene();
    if (!desc.scenePath.empty()) {
      Log(LogLevel::Warning, "Cena nao carregada (%s); usando cena padrao", world.LastError().c_str());
    }
  }
  loop.SetFixedStep(desc.fixedStep);
  loop.SetMaxStepsPerFrame(desc.maxStepsPerFrame);
  window.SetResizeCallback([this](int w, int h) {
    if (rhi != nullptr) {
      rhi->Resize(w, h);
    }
    world.SetViewSize(w, h);
  });
  initialized = true;
  return true;
}
void GameApp::Shutdown() {
  if (!initialized) {
    return;
  }
  initialized = false;
  running = false;
  world.Shutdown();
  if (rhi != nullptr) {
    DestroyRHI(rhi);
    rhi = nullptr;
  }
  RawInputPoll::Shutdown();
  window.Destroy();
}
void GameApp::UpdateInput() {
  if (!window.IsOpen()) {
    loop.RequestQuit();
    return;
  }
  if (RawInputPoll::IsKeyDown(VK_ESCAPE)) {
    loop.RequestQuit();
  }
  const float moveSpeed = 12.0f;
  const float lookSpeed = 0.0025f;
  int mdx = 0;
  int mdy = 0;
  RawInputPoll::GetMouseDelta(mdx, mdy);
  camYaw += static_cast<float>(mdx) * lookSpeed;
  camPitch -= static_cast<float>(mdy) * lookSpeed;
  if (camPitch > 1.55f) {
    camPitch = 1.55f;
  }
  if (camPitch < -1.55f) {
    camPitch = -1.55f;
  }
  float px = 0.0f;
  float py = 0.0f;
  float pz = 0.0f;
  world.GetCamera().GetPosition(px, py, pz);
  const float fx = std::sin(camYaw) * std::cos(camPitch);
  const float fy = -std::sin(camPitch);
  const float fz = std::cos(camYaw) * std::cos(camPitch);
  const float rx = std::cos(camYaw);
  const float rz = -std::sin(camYaw);
  float move = 0.0f;
  float strafe = 0.0f;
  float rise = 0.0f;
  if (RawInputPoll::IsKeyDown('W')) {
    move += 1.0f;
  }
  if (RawInputPoll::IsKeyDown('S')) {
    move -= 1.0f;
  }
  if (RawInputPoll::IsKeyDown('D')) {
    strafe += 1.0f;
  }
  if (RawInputPoll::IsKeyDown('A')) {
    strafe -= 1.0f;
  }
  if (RawInputPoll::IsKeyDown('E')) {
    rise += 1.0f;
  }
  if (RawInputPoll::IsKeyDown('Q')) {
    rise -= 1.0f;
  }
  const float step = moveSpeed * loop.FixedStep();
  px += (fx * move + rx * strafe) * step;
  py += (fy * move + rise) * step;
  pz += (fz * move + rz * strafe) * step;
  world.SetCameraPosition(px, py, pz);
  world.SetCameraYawPitch(camYaw, camPitch);
}
void GameApp::HandleResize() {
  int cw = 0;
  int ch = 0;
  window.ClientSize(cw, ch);
  if (cw > 0 && ch > 0 && rhi != nullptr) {
    if (cw != rhi->Width() || ch != rhi->Height()) {
      rhi->Resize(cw, ch);
    }
    world.SetViewSize(cw, ch);
  }
}
int GameApp::Run() {
  if (!initialized) {
    return 1;
  }
  loop.AddUpdateSystem("input", [this](float dt, float alpha) {
    (void)dt;
    (void)alpha;
    UpdateInput();
  });
  loop.AddUpdateSystem("physics", [this](float dt, float alpha) {
    (void)alpha;
    world.SyncPhysics(dt);
  });
  loop.AddUpdateSystem("audio", [this](float dt, float alpha) {
    (void)dt;
    (void)alpha;
    world.SyncAudio(dt);
  });
  loop.AddRenderSystem("render", [this](float alpha) {
    world.Render(alpha);
  });
  running = true;
  uint64_t last = Clock::Millis();
  while (window.IsOpen() && window.PollEvents() && !loop.QuitRequested()) {
    RawInputPoll::Poll();
    HandleResize();
    const uint64_t now = Clock::Millis();
    float dt = static_cast<float>(now - last) / 1000.0f;
    last = now;
    loop.Tick(dt);
    if (rhi != nullptr) {
      rhi->Present(desc.vsync);
    }
  }
  running = false;
  return 0;
}
}