#include <caligo/platform/Input.h>
#include <cstdint>
#include <doctest/doctest.h>

TEST_CASE("input tracks key transitions") {
  caligo::platform::Input input;

  input.NewFrame();
  input.ApplyKeyDown(caligo::platform::Key::A);
  CHECK(input.State().IsKeyDown(caligo::platform::Key::A));
  CHECK(input.State().WasKeyPressed(caligo::platform::Key::A));
  CHECK_FALSE(input.State().WasKeyReleased(caligo::platform::Key::A));

  input.NewFrame();
  CHECK(input.State().IsKeyDown(caligo::platform::Key::A));
  CHECK_FALSE(input.State().WasKeyPressed(caligo::platform::Key::A));

  input.ApplyKeyDown(caligo::platform::Key::A);
  CHECK_FALSE(input.State().WasKeyPressed(caligo::platform::Key::A));

  input.ApplyKeyUp(caligo::platform::Key::A);
  CHECK_FALSE(input.State().IsKeyDown(caligo::platform::Key::A));
  CHECK(input.State().WasKeyReleased(caligo::platform::Key::A));

  input.NewFrame();
  CHECK_FALSE(input.State().WasKeyReleased(caligo::platform::Key::A));
}

TEST_CASE("input tracks mouse state") {
  caligo::platform::Input input;

  input.NewFrame();
  input.ApplyMouseButtonDown(caligo::platform::MouseButton::Left);
  CHECK(input.State().IsMouseDown(caligo::platform::MouseButton::Left));
  CHECK(input.State().WasMousePressed(caligo::platform::MouseButton::Left));

  input.ApplyMouseMove(10.0f, 20.0f);
  input.ApplyMouseDelta(2.0f, 3.0f);
  input.ApplyWheel(1.5f);
  CHECK(input.State().MouseX() == doctest::Approx(10.0f));
  CHECK(input.State().MouseY() == doctest::Approx(20.0f));
  CHECK(input.State().MouseDeltaX() == doctest::Approx(2.0f));
  CHECK(input.State().MouseDeltaY() == doctest::Approx(3.0f));
  CHECK(input.State().WheelDelta() == doctest::Approx(1.5f));

  input.NewFrame();
  CHECK(input.State().IsMouseDown(caligo::platform::MouseButton::Left));
  CHECK_FALSE(input.State().WasMousePressed(caligo::platform::MouseButton::Left));
  CHECK(input.State().MouseDeltaX() == 0.0f);
  CHECK(input.State().MouseDeltaY() == 0.0f);
  CHECK(input.State().WheelDelta() == 0.0f);

  input.ApplyMouseButtonUp(caligo::platform::MouseButton::Left);
  CHECK_FALSE(input.State().IsMouseDown(caligo::platform::MouseButton::Left));
  CHECK(input.State().WasMouseReleased(caligo::platform::MouseButton::Left));

  input.Reset();
  CHECK_FALSE(input.State().IsMouseDown(caligo::platform::MouseButton::Left));
  CHECK(input.State().MouseX() == 0.0f);
}

TEST_CASE("virtual key translation is stable") {
  using caligo::platform::FromVirtualKey;
  using caligo::platform::Key;

  CHECK(FromVirtualKey('A', 0x1E, false) == Key::A);
  CHECK(FromVirtualKey('Z', 0x2C, false) == Key::Z);
  CHECK(FromVirtualKey('0', 0x0B, false) == Key::Digit0);
  CHECK(FromVirtualKey(0x70, 0x3B, false) == Key::F1);
  CHECK(FromVirtualKey(0x25, 0x4B, true) == Key::Left);
  CHECK(FromVirtualKey(0x10, 0x2A, false) == Key::LeftShift);
  CHECK(FromVirtualKey(0x10, 0x36, false) == Key::RightShift);
  CHECK(FromVirtualKey(0x11, 0x1D, true) == Key::RightControl);
  CHECK(FromVirtualKey(0x0D, 0x1C, true) == Key::NumpadEnter);
  CHECK(FromVirtualKey(0xFF, 0x00, false) == Key::Unknown);
}
