#include <caligo/platform/Input.h>
#include <windows.h>

namespace caligo::platform {

bool InputState::IsKeyDown(Key key) const noexcept {
  const size_t index = static_cast<size_t>(key);
  return index < kKeyCount && m_keysDown[index];
}

bool InputState::WasKeyPressed(Key key) const noexcept {
  const size_t index = static_cast<size_t>(key);
  return index < kKeyCount && m_keysPressed[index];
}

bool InputState::WasKeyReleased(Key key) const noexcept {
  const size_t index = static_cast<size_t>(key);
  return index < kKeyCount && m_keysReleased[index];
}

bool InputState::IsMouseDown(MouseButton button) const noexcept {
  const size_t index = static_cast<size_t>(button);
  return index < kMouseButtonCount && m_mouseDown[index];
}

bool InputState::WasMousePressed(MouseButton button) const noexcept {
  const size_t index = static_cast<size_t>(button);
  return index < kMouseButtonCount && m_mousePressed[index];
}

bool InputState::WasMouseReleased(MouseButton button) const noexcept {
  const size_t index = static_cast<size_t>(button);
  return index < kMouseButtonCount && m_mouseReleased[index];
}

float InputState::MouseX() const noexcept {
  return m_mouseX;
}

float InputState::MouseY() const noexcept {
  return m_mouseY;
}

float InputState::MouseDeltaX() const noexcept {
  return m_mouseDeltaX;
}

float InputState::MouseDeltaY() const noexcept {
  return m_mouseDeltaY;
}

float InputState::WheelDelta() const noexcept {
  return m_wheelDelta;
}

void InputState::ApplyKeyDown(Key key) noexcept {
  const size_t index = static_cast<size_t>(key);
  if (index >= kKeyCount || m_keysDown[index]) {
    return;
  }
  m_keysDown[index] = true;
  m_keysPressed[index] = true;
}

void InputState::ApplyKeyUp(Key key) noexcept {
  const size_t index = static_cast<size_t>(key);
  if (index >= kKeyCount || !m_keysDown[index]) {
    return;
  }
  m_keysDown[index] = false;
  m_keysReleased[index] = true;
}

void InputState::ApplyMouseButtonDown(MouseButton button) noexcept {
  const size_t index = static_cast<size_t>(button);
  if (index >= kMouseButtonCount || m_mouseDown[index]) {
    return;
  }
  m_mouseDown[index] = true;
  m_mousePressed[index] = true;
}

void InputState::ApplyMouseButtonUp(MouseButton button) noexcept {
  const size_t index = static_cast<size_t>(button);
  if (index >= kMouseButtonCount || !m_mouseDown[index]) {
    return;
  }
  m_mouseDown[index] = false;
  m_mouseReleased[index] = true;
}

void InputState::ApplyMouseMove(float x, float y) noexcept {
  m_mouseX = x;
  m_mouseY = y;
}

void InputState::ApplyMouseDelta(float deltaX, float deltaY) noexcept {
  m_mouseDeltaX += deltaX;
  m_mouseDeltaY += deltaY;
}

void InputState::ApplyWheel(float delta) noexcept {
  m_wheelDelta += delta;
}

void InputState::BeginFrame() noexcept {
  m_keysPressed.fill(false);
  m_keysReleased.fill(false);
  m_mousePressed.fill(false);
  m_mouseReleased.fill(false);
  m_mouseDeltaX = 0.0f;
  m_mouseDeltaY = 0.0f;
  m_wheelDelta = 0.0f;
}

void InputState::Reset() noexcept {
  *this = InputState{};
}

void Input::NewFrame() noexcept {
  m_state.BeginFrame();
}

void Input::Reset() noexcept {
  m_state.Reset();
}

void Input::ApplyKeyDown(Key key) noexcept {
  m_state.ApplyKeyDown(key);
}

void Input::ApplyKeyUp(Key key) noexcept {
  m_state.ApplyKeyUp(key);
}

void Input::ApplyMouseButtonDown(MouseButton button) noexcept {
  m_state.ApplyMouseButtonDown(button);
}

void Input::ApplyMouseButtonUp(MouseButton button) noexcept {
  m_state.ApplyMouseButtonUp(button);
}

void Input::ApplyMouseMove(float x, float y) noexcept {
  m_state.ApplyMouseMove(x, y);
}

void Input::ApplyMouseDelta(float deltaX, float deltaY) noexcept {
  m_state.ApplyMouseDelta(deltaX, deltaY);
}

void Input::ApplyWheel(float delta) noexcept {
  m_state.ApplyWheel(delta);
}

const InputState& Input::State() const noexcept {
  return m_state;
}

Key FromVirtualKey(uint32_t virtualKey, uint32_t scanCode, bool extended) noexcept {
  if (virtualKey >= 'A' && virtualKey <= 'Z') {
    return static_cast<Key>(static_cast<uint32_t>(Key::A) + (virtualKey - 'A'));
  }
  if (virtualKey >= '0' && virtualKey <= '9') {
    return static_cast<Key>(static_cast<uint32_t>(Key::Digit0) + (virtualKey - '0'));
  }
  if (virtualKey >= VK_F1 && virtualKey <= VK_F12) {
    return static_cast<Key>(static_cast<uint32_t>(Key::F1) + (virtualKey - VK_F1));
  }
  if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
    return static_cast<Key>(static_cast<uint32_t>(Key::Numpad0) + (virtualKey - VK_NUMPAD0));
  }

  switch (virtualKey) {
  case VK_ESCAPE:
    return Key::Escape;
  case VK_RETURN:
    return extended ? Key::NumpadEnter : Key::Enter;
  case VK_TAB:
    return Key::Tab;
  case VK_SPACE:
    return Key::Space;
  case VK_BACK:
    return Key::Backspace;
  case VK_INSERT:
    return Key::Insert;
  case VK_DELETE:
    return Key::Delete;
  case VK_HOME:
    return Key::Home;
  case VK_END:
    return Key::End;
  case VK_PRIOR:
    return Key::PageUp;
  case VK_NEXT:
    return Key::PageDown;
  case VK_LEFT:
    return Key::Left;
  case VK_RIGHT:
    return Key::Right;
  case VK_UP:
    return Key::Up;
  case VK_DOWN:
    return Key::Down;
  case VK_SHIFT:
    return scanCode == 0x36 ? Key::RightShift : Key::LeftShift;
  case VK_LSHIFT:
    return Key::LeftShift;
  case VK_RSHIFT:
    return Key::RightShift;
  case VK_CONTROL:
    return extended ? Key::RightControl : Key::LeftControl;
  case VK_LCONTROL:
    return Key::LeftControl;
  case VK_RCONTROL:
    return Key::RightControl;
  case VK_MENU:
    return extended ? Key::RightAlt : Key::LeftAlt;
  case VK_LMENU:
    return Key::LeftAlt;
  case VK_RMENU:
    return Key::RightAlt;
  case VK_LWIN:
    return Key::LeftSuper;
  case VK_RWIN:
    return Key::RightSuper;
  case VK_OEM_MINUS:
    return Key::Minus;
  case VK_OEM_PLUS:
    return Key::Equal;
  case VK_OEM_4:
    return Key::LeftBracket;
  case VK_OEM_6:
    return Key::RightBracket;
  case VK_OEM_5:
    return Key::Backslash;
  case VK_OEM_1:
    return Key::Semicolon;
  case VK_OEM_7:
    return Key::Apostrophe;
  case VK_OEM_3:
    return Key::Grave;
  case VK_OEM_COMMA:
    return Key::Comma;
  case VK_OEM_PERIOD:
    return Key::Period;
  case VK_OEM_2:
    return Key::Slash;
  case VK_CAPITAL:
    return Key::CapsLock;
  case VK_NUMLOCK:
    return Key::NumLock;
  case VK_SCROLL:
    return Key::ScrollLock;
  case VK_SNAPSHOT:
    return Key::PrintScreen;
  case VK_PAUSE:
    return Key::Pause;
  case VK_MULTIPLY:
    return Key::NumpadMultiply;
  case VK_ADD:
    return Key::NumpadAdd;
  case VK_SUBTRACT:
    return Key::NumpadSubtract;
  case VK_DECIMAL:
    return Key::NumpadDecimal;
  case VK_DIVIDE:
    return Key::NumpadDivide;
  default:
    return Key::Unknown;
  }
}

}
