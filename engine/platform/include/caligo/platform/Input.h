#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace caligo::platform {

enum class Key : uint8_t {
  Unknown = 0,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  Numpad0,
  Numpad1,
  Numpad2,
  Numpad3,
  Numpad4,
  Numpad5,
  Numpad6,
  Numpad7,
  Numpad8,
  Numpad9,
  Escape,
  Enter,
  Tab,
  Space,
  Backspace,
  Insert,
  Delete,
  Home,
  End,
  PageUp,
  PageDown,
  Left,
  Right,
  Up,
  Down,
  LeftShift,
  RightShift,
  LeftControl,
  RightControl,
  LeftAlt,
  RightAlt,
  LeftSuper,
  RightSuper,
  Minus,
  Equal,
  LeftBracket,
  RightBracket,
  Backslash,
  Semicolon,
  Apostrophe,
  Grave,
  Comma,
  Period,
  Slash,
  CapsLock,
  NumLock,
  ScrollLock,
  PrintScreen,
  Pause,
  NumpadMultiply,
  NumpadAdd,
  NumpadSubtract,
  NumpadDecimal,
  NumpadDivide,
  NumpadEnter,
  Count
};

enum class MouseButton : uint8_t { Left, Right, Middle, X1, X2, Count };

inline constexpr size_t kKeyCount = static_cast<size_t>(Key::Count);
inline constexpr size_t kMouseButtonCount = static_cast<size_t>(MouseButton::Count);

class Input;

class InputState final {
public:
  [[nodiscard]] bool IsKeyDown(Key key) const noexcept;
  [[nodiscard]] bool WasKeyPressed(Key key) const noexcept;
  [[nodiscard]] bool WasKeyReleased(Key key) const noexcept;

  [[nodiscard]] bool IsMouseDown(MouseButton button) const noexcept;
  [[nodiscard]] bool WasMousePressed(MouseButton button) const noexcept;
  [[nodiscard]] bool WasMouseReleased(MouseButton button) const noexcept;

  [[nodiscard]] float MouseX() const noexcept;
  [[nodiscard]] float MouseY() const noexcept;
  [[nodiscard]] float MouseDeltaX() const noexcept;
  [[nodiscard]] float MouseDeltaY() const noexcept;
  [[nodiscard]] float WheelDelta() const noexcept;

private:
  friend class Input;

  void ApplyKeyDown(Key key) noexcept;
  void ApplyKeyUp(Key key) noexcept;
  void ApplyMouseButtonDown(MouseButton button) noexcept;
  void ApplyMouseButtonUp(MouseButton button) noexcept;
  void ApplyMouseMove(float x, float y) noexcept;
  void ApplyMouseDelta(float deltaX, float deltaY) noexcept;
  void ApplyWheel(float delta) noexcept;
  void BeginFrame() noexcept;
  void Reset() noexcept;

  std::array<bool, kKeyCount> m_keysDown{};
  std::array<bool, kKeyCount> m_keysPressed{};
  std::array<bool, kKeyCount> m_keysReleased{};
  std::array<bool, kMouseButtonCount> m_mouseDown{};
  std::array<bool, kMouseButtonCount> m_mousePressed{};
  std::array<bool, kMouseButtonCount> m_mouseReleased{};
  float m_mouseX = 0.0f;
  float m_mouseY = 0.0f;
  float m_mouseDeltaX = 0.0f;
  float m_mouseDeltaY = 0.0f;
  float m_wheelDelta = 0.0f;
};

class Input final {
public:
  void NewFrame() noexcept;
  void Reset() noexcept;

  void ApplyKeyDown(Key key) noexcept;
  void ApplyKeyUp(Key key) noexcept;
  void ApplyMouseButtonDown(MouseButton button) noexcept;
  void ApplyMouseButtonUp(MouseButton button) noexcept;
  void ApplyMouseMove(float x, float y) noexcept;
  void ApplyMouseDelta(float deltaX, float deltaY) noexcept;
  void ApplyWheel(float delta) noexcept;

  [[nodiscard]] const InputState& State() const noexcept;

private:
  InputState m_state;
};

[[nodiscard]] Key FromVirtualKey(uint32_t virtualKey, uint32_t scanCode, bool extended) noexcept;

}
