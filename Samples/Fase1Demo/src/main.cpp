#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Window.h"
#include "Kizuri/Input.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/RHI.h"
#include <TaskScheduler.h>
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <atomic>
namespace {
struct FrameJob : public enki::ITaskSet {
  std::atomic<int>* counter;
  FrameJob(std::atomic<int>* c) : counter(c) { m_SetSize = 4; }
  void ExecuteRange(enki::TaskSetPartition range, uint32_t) override {
    for (uint32_t i = range.start; i < range.end; ++i) {
      (void)i;
      counter->fetch_add(1);
    }
  }
};
}
int main() {
  std::printf("Kizuri Fase1Demo\n");
  Kizuri::JobSystem js;
  if (!js.Initialize()) {
    std::printf("JobSystem init failed\n");
    return 1;
  }
  std::printf("Job threads: %u\n", js.GetThreadCount());
  Kizuri::Window win;
  if (!win.Create(L"Kizuri Fase1 - Window+Job+RHI DX11", 1280, 720)) {
    std::printf("Window create failed\n");
    return 1;
  }
  Kizuri::RawInputPoll::Initialize(win.NativeHandle());
  Kizuri::RHIDesc desc;
  desc.windowHandle = win.NativeHandle();
  desc.width = win.Width();
  desc.height = win.Height();
  desc.vsync = true;
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::D3D11);
  if (rhi == nullptr) {
    std::printf("RHI create failed\n");
    return 1;
  }
  if (!rhi->Initialize(desc)) {
    std::printf("RHI DX11 init failed\n");
    const char* reason = rhi->LastError();
    if (reason != nullptr && reason[0] != '\0') {
      std::printf("Detalhe: %s\n", reason);
    }
    return 1;
  }
  std::printf("RHI backend: %s\n", rhi->BackendName());
  std::atomic<int> jobsDone(0);
  unsigned long long start = GetTickCount64();
  int frames = 0;
  while (win.PollEvents()) {
    Kizuri::RawInputPoll::Poll();
    if (Kizuri::RawInputPoll::IsKeyDown(VK_ESCAPE)) {
      break;
    }
    bool dodge = Kizuri::RawInputPoll::IsKeyDown(VK_SPACE);
    bool attack = Kizuri::RawInputPoll::IsKeyDown(VK_LBUTTON) || Kizuri::RawInputPoll::IsKeyDown(0x46);
    (void)dodge;
    (void)attack;
    FrameJob job(&jobsDone);
    js.AddTask(&job);
    unsigned long long now = GetTickCount64();
    float t = static_cast<float>(now - start) / 1000.0f;
    float r = 0.08f + 0.07f * sinf(t * 0.8f);
    float g = 0.10f + 0.07f * sinf(t * 0.6f + 2.0f);
    float b = 0.25f + 0.10f * sinf(t * 0.5f + 4.0f);
    rhi->Clear(r, g, b, 1.0f);
    rhi->Present(true);
    js.WaitForTask(&job);
    ++frames;
    if (frames == 60) {
      std::printf("60 frames ok, jobsDone=%d dodge=%d attack=%d\n", jobsDone.load(), dodge ? 1 : 0, attack ? 1 : 0);
    }
    if (frames >= 600) {
      break;
    }
  }
  std::printf("frames=%d jobsDone=%d\n", frames, jobsDone.load());
  Kizuri::DestroyRHI(rhi);
  Kizuri::RawInputPoll::Shutdown();
  win.Destroy();
  js.Shutdown();
  return 0;
}
