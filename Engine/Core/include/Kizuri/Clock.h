#pragma once
#include <stdint.h>
namespace Kizuri {
class Clock {
public:
  static double Seconds();
  static uint64_t Millis();
  static uint64_t Micros();
private:
  Clock() = delete;
};
class Stopwatch {
public:
  Stopwatch();
  void Start();
  void Stop();
  double ElapsedSeconds() const;
  double ElapsedMs() const;
  double Restart();
  bool Running() const;
private:
  uint64_t startTicks;
  uint64_t accumTicks;
  bool running;
  static uint64_t NowTicks();
  static double TicksToSeconds(uint64_t ticks);
};
}
