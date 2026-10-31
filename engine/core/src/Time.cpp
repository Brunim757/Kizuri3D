#include <algorithm>
#include <caligo/core/Assert.h>
#include <caligo/core/Time.h>
#include <chrono>

namespace caligo {

namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr double kDefaultTicksPerSecond = 60.0;
constexpr double kDefaultMaxFrameDelta = 0.25;

}

Clock::Clock() {
  m_last =
      std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double Clock::NowSeconds() const noexcept {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double Clock::DeltaSeconds() noexcept {
  const double now = NowSeconds();
  const double delta = now - m_last;
  m_last = now;
  return delta > 0.0 ? delta : 0.0;
}

FrameTimer::FrameTimer(double ticksPerSecond, double maxFrameDelta)
    : m_fixedDelta(ticksPerSecond > 0.0 ? 1.0 / ticksPerSecond : 1.0 / kDefaultTicksPerSecond),
      m_maxFrameDelta(maxFrameDelta > 0.0 ? maxFrameDelta : kDefaultMaxFrameDelta) {
  CALIGO_ASSERT_MSG(ticksPerSecond > 0.0, "ticks per second must be positive");
  CALIGO_ASSERT_MSG(maxFrameDelta > 0.0, "max frame delta must be positive");
}

void FrameTimer::Reset() noexcept {
  m_accumulator = 0.0;
  m_tickCount = 0;
}

FrameStep FrameTimer::Advance(double frameDelta) noexcept {
  const double clamped = std::clamp(frameDelta, 0.0, m_maxFrameDelta);
  m_accumulator += clamped;

  FrameStep result{};
  result.fixedDelta = m_fixedDelta;
  result.frameDelta = clamped;

  while (m_accumulator + kEpsilon >= m_fixedDelta) {
    m_accumulator -= m_fixedDelta;
    ++m_tickCount;
    ++result.steps;
  }

  if (m_accumulator < 0.0) {
    m_accumulator = 0.0;
  }

  result.alpha = m_accumulator / m_fixedDelta;
  return result;
}

double FrameTimer::FixedDelta() const noexcept {
  return m_fixedDelta;
}

uint64_t FrameTimer::TickCount() const noexcept {
  return m_tickCount;
}

}
