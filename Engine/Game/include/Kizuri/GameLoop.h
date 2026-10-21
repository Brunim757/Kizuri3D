#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace Kizuri {
enum class GameState : int {
  Init = 0,
  Running = 1,
  Paused = 2,
  Stopping = 3
};
class GameLoop {
public:
  using UpdateFn = std::function<void(float dt, float alpha)>;
  using RenderFn = std::function<void(float alpha)>;
  struct SystemStat {
    std::string name;
    float ms = 0.0f;
  };
  GameLoop();
  void SetFixedStep(float seconds);
  void SetMaxStepsPerFrame(int steps);
  float FixedStep() const { return fixedStep; }
  int MaxStepsPerFrame() const { return maxSteps; }
  void AddUpdateSystem(const std::string& name, UpdateFn fn);
  void AddRenderSystem(const std::string& name, RenderFn fn);
  void Tick(float realDeltaSeconds);
  GameState State() const { return state; }
  void SetState(GameState s) { state = s; }
  void RequestQuit() { quitRequested = true; }
  bool QuitRequested() const { return quitRequested; }
  float Alpha() const { return alpha; }
  float SimulationTime() const { return simTime; }
  uint64_t StepCount() const { return stepCount; }
  uint64_t FrameCount() const { return frameCount; }
  float LastFrameMs() const { return lastFrameMs; }
  const std::vector<SystemStat>& UpdateStats() const { return updateStats; }
  const std::vector<SystemStat>& RenderStats() const { return renderStats; }
private:
  struct UpdateEntry {
    std::string name;
    UpdateFn fn;
  };
  struct RenderEntry {
    std::string name;
    RenderFn fn;
  };
  float fixedStep = 1.0f / 60.0f;
  int maxSteps = 4;
  float accumulator = 0.0f;
  float alpha = 0.0f;
  float simTime = 0.0f;
  uint64_t stepCount = 0;
  uint64_t frameCount = 0;
  GameState state = GameState::Running;
  bool quitRequested = false;
  float lastFrameMs = 0.0f;
  std::vector<UpdateEntry> updates;
  std::vector<RenderEntry> renders;
  std::vector<SystemStat> updateStats;
  std::vector<SystemStat> renderStats;
};
}