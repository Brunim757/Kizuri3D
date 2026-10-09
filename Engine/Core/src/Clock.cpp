#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Clock.h"
#include <windows.h>
namespace Kizuri {
namespace {
double TicksPerSecond() {
  LARGE_INTEGER f;
  if (QueryPerformanceFrequency(&f) == FALSE || f.QuadPart <= 0) {
    return 1000.0;
  }
  return static_cast<double>(f.QuadPart);
}
uint64_t RawTicks() {
  LARGE_INTEGER c;
  if (QueryPerformanceCounter(&c) == FALSE) {
    return GetTickCount64() * 10000ULL;
  }
  return static_cast<uint64_t>(c.QuadPart);
}
}
double Clock::Seconds() {
  return static_cast<double>(RawTicks()) / TicksPerSecond();
}
uint64_t Clock::Millis() {
  return static_cast<uint64_t>(Seconds() * 1000.0);
}
uint64_t Clock::Micros() {
  return static_cast<uint64_t>(Seconds() * 1000000.0);
}
uint64_t Stopwatch::NowTicks() {
  return RawTicks();
}
double Stopwatch::TicksToSeconds(uint64_t ticks) {
  return static_cast<double>(ticks) / TicksPerSecond();
}
Stopwatch::Stopwatch()
  : startTicks(0)
  , accumTicks(0)
  , running(false) {
}
void Stopwatch::Start() {
  if (running) {
    return;
  }
  startTicks = NowTicks();
  running = true;
}
void Stopwatch::Stop() {
  if (!running) {
    return;
  }
  accumTicks += NowTicks() - startTicks;
  running = false;
}
double Stopwatch::ElapsedSeconds() const {
  uint64_t total = accumTicks;
  if (running) {
    total += NowTicks() - startTicks;
  }
  return TicksToSeconds(total);
}
double Stopwatch::ElapsedMs() const {
  return ElapsedSeconds() * 1000.0;
}
double Stopwatch::Restart() {
  double e = ElapsedSeconds();
  accumTicks = 0;
  startTicks = NowTicks();
  running = true;
  return e;
}
bool Stopwatch::Running() const {
  return running;
}
}
