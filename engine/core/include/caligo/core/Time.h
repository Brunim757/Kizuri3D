#pragma once

#include <cstdint>

namespace caligo {

struct FrameStep {
  uint32_t steps = 0;
  double fixedDelta = 0.0;
  double alpha = 0.0;
  double frameDelta = 0.0;
};

class Clock final {
public:
  Clock();

  [[nodiscard]] double NowSeconds() const noexcept;
  [[nodiscard]] double DeltaSeconds() noexcept;

private:
  double m_last = 0.0;
};

class FrameTimer final {
public:
  explicit FrameTimer(double ticksPerSecond = 60.0, double maxFrameDelta = 0.25);

  void Reset() noexcept;
  [[nodiscard]] FrameStep Advance(double frameDelta) noexcept;

  [[nodiscard]] double FixedDelta() const noexcept;
  [[nodiscard]] uint64_t TickCount() const noexcept;

private:
  double m_fixedDelta = 0.0;
  double m_maxFrameDelta = 0.0;
  double m_accumulator = 0.0;
  uint64_t m_tickCount = 0;
};

}
