#pragma once
#include <chrono>
#include <thread>
namespace Kizuri {
class FramePacer {
public:
  FramePacer()
    : targetFps(0.0f)
    , last(std::chrono::steady_clock::now())
    , lastWaitMs(0.0) {
  }
  void SetTargetFps(float fps) {
    targetFps = fps < 0.0f ? 0.0f : fps;
  }
  float TargetFps() const {
    return targetFps;
  }
  double LastWaitMs() const {
    return lastWaitMs;
  }
  void Wait() {
    auto now = std::chrono::steady_clock::now();
    lastWaitMs = 0.0;
    if (targetFps > 0.5f) {
      double periodMs = 1000.0 / static_cast<double>(targetFps);
      auto due = last + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::milli>(periodMs));
      if (now < due) {
        std::this_thread::sleep_until(due);
        lastWaitMs = periodMs;
      }
    }
    last = std::chrono::steady_clock::now();
  }
private:
  float targetFps;
  std::chrono::steady_clock::time_point last;
  double lastWaitMs;
};
}
