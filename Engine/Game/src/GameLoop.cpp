#include "Kizuri/GameLoop.h"
#include "Kizuri/Clock.h"
#include <algorithm>
namespace Kizuri {
GameLoop::GameLoop() {
}
void GameLoop::SetFixedStep(float seconds) {
  if (seconds > 0.0f && seconds < 1.0f) {
    fixedStep = seconds;
  }
}
void GameLoop::SetMaxStepsPerFrame(int steps) {
  if (steps > 0 && steps < 64) {
    maxSteps = steps;
  }
}
void GameLoop::AddUpdateSystem(const std::string& name, UpdateFn fn) {
  if (!fn) {
    return;
  }
  UpdateEntry e;
  e.name = name;
  e.fn = fn;
  updates.push_back(e);
}
void GameLoop::AddRenderSystem(const std::string& name, RenderFn fn) {
  if (!fn) {
    return;
  }
  RenderEntry e;
  e.name = name;
  e.fn = fn;
  renders.push_back(e);
}
void GameLoop::Tick(float realDeltaSeconds) {
  const double frameStart = Clock::Seconds();
  float dt = realDeltaSeconds;
  if (dt < 0.0f) {
    dt = 0.0f;
  }
  const float maxDelta = fixedStep * static_cast<float>(maxSteps);
  if (dt > maxDelta) {
    dt = maxDelta;
  }
  if (state == GameState::Running) {
    accumulator += dt;
    int steps = 0;
    while (accumulator >= fixedStep && steps < maxSteps) {
      updateStats.resize(updates.size());
      for (size_t i = 0; i < updates.size(); ++i) {
        const double t0 = Clock::Seconds();
        updates[i].fn(fixedStep, alpha);
        updateStats[i].name = updates[i].name;
        updateStats[i].ms = static_cast<float>((Clock::Seconds() - t0) * 1000.0);
      }
      simTime += fixedStep;
      ++stepCount;
      accumulator -= fixedStep;
      ++steps;
    }
    if (steps >= maxSteps && accumulator >= fixedStep) {
      accumulator = 0.0f;
    }
    alpha = fixedStep > 0.0f ? accumulator / fixedStep : 0.0f;
  }
  renderStats.resize(renders.size());
  for (size_t i = 0; i < renders.size(); ++i) {
    const double t0 = Clock::Seconds();
    renders[i].fn(alpha);
    renderStats[i].name = renders[i].name;
    renderStats[i].ms = static_cast<float>((Clock::Seconds() - t0) * 1000.0);
  }
  ++frameCount;
  lastFrameMs = static_cast<float>((Clock::Seconds() - frameStart) * 1000.0);
}
}