#pragma once
namespace Kizuri {
struct GamepadState {
  bool connected = false;
  unsigned int buttons = 0;
  float leftX = 0.0f;
  float leftY = 0.0f;
  float rightX = 0.0f;
  float rightY = 0.0f;
  float leftTrigger = 0.0f;
  float rightTrigger = 0.0f;
};
class RawInputPoll {
public:
  static bool Initialize(void* hwnd);
  static void Shutdown();
  static void Poll();
  static bool IsKeyDown(int vk);
  static bool WasKeyPressed(int vk);
  static bool WasKeyReleased(int vk);
  static bool IsMouseDown(int vk);
  static void GetMousePosition(int& x, int& y);
  static void GetMouseClientPosition(int& x, int& y);
  static void GetMouseDelta(int& dx, int& dy);
  static int GetMouseWheelDelta();
  static void OnRawMouseDelta(int dx, int dy);
  static void OnMouseWheel(int delta);
  static bool IsGamepadConnected();
  static GamepadState GetGamepadState();
private:
  RawInputPoll() = delete;
};
}
