#include <caligo/core/Time.h>
#include <doctest/doctest.h>

TEST_CASE("frame timer advances one fixed step per frame") {
  caligo::FrameTimer timer(60.0, 0.25);
  const caligo::FrameStep step = timer.Advance(1.0 / 60.0);

  CHECK(step.steps == 1);
  CHECK(step.fixedDelta == doctest::Approx(1.0 / 60.0));
  CHECK(step.alpha < 1.0e-9);
  CHECK(timer.TickCount() == 1);
}

TEST_CASE("frame timer accumulates partial frames") {
  caligo::FrameTimer timer(60.0, 0.25);
  const caligo::FrameStep first = timer.Advance(1.0 / 120.0);
  CHECK(first.steps == 0);
  CHECK(first.alpha > 0.49);
  CHECK(first.alpha < 0.51);

  const caligo::FrameStep second = timer.Advance(1.0 / 120.0);
  CHECK(second.steps == 1);
  CHECK(timer.TickCount() == 1);
}

TEST_CASE("frame timer clamps long frames") {
  caligo::FrameTimer timer(60.0, 0.25);
  const caligo::FrameStep step = timer.Advance(5.0);

  CHECK(step.frameDelta == doctest::Approx(0.25));
  CHECK(step.steps == 15);
  CHECK(timer.TickCount() == 15);
}

TEST_CASE("frame timer ignores negative frames and resets") {
  caligo::FrameTimer timer(60.0, 0.25);
  const caligo::FrameStep step = timer.Advance(-1.0);

  CHECK(step.frameDelta == 0.0);
  CHECK(step.steps == 0);
  CHECK(timer.TickCount() == 0);

  timer.Advance(1.0 / 60.0);
  CHECK(timer.TickCount() == 1);
  timer.Reset();
  CHECK(timer.TickCount() == 0);
}

TEST_CASE("clock is monotonic") {
  caligo::Clock clock;
  const double first = clock.NowSeconds();
  const double delta = clock.DeltaSeconds();

  CHECK(delta >= 0.0);
  CHECK(clock.NowSeconds() >= first);
}
