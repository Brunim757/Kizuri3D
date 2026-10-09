#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Input.h"
#include <windows.h>
#include <xinput.h>
#include <atomic>
#include <cstring>
namespace Kizuri {
namespace {
bool g_inputReady = false;
HWND g_hwnd = nullptr;
std::atomic<int> g_dx(0);
std::atomic<int> g_dy(0);
std::atomic<int> g_wheel(0);
unsigned char g_keysNow[256] = { 0 };
unsigned char g_keysPrev[256] = { 0 };
}
bool RawInputPoll::Initialize(void* hwnd) {
  g_hwnd = static_cast<HWND>(hwnd);
  RAWINPUTDEVICE rid[2];
  rid[0].usUsagePage = 0x01;
  rid[0].usUsage = 0x02;
  rid[0].dwFlags = RIDEV_INPUTSINK;
  rid[0].hwndTarget = g_hwnd;
  rid[1].usUsagePage = 0x01;
  rid[1].usUsage = 0x06;
  rid[1].dwFlags = RIDEV_INPUTSINK;
  rid[1].hwndTarget = g_hwnd;
  BOOL ok = RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
  g_inputReady = (ok == TRUE);
  std::memset(g_keysNow, 0, sizeof(g_keysNow));
  std::memset(g_keysPrev, 0, sizeof(g_keysPrev));
  return g_inputReady;
}
void RawInputPoll::Shutdown() {
  g_inputReady = false;
  g_hwnd = nullptr;
}
void RawInputPoll::Poll() {
  std::memcpy(g_keysPrev, g_keysNow, sizeof(g_keysPrev));
  for (int vk = 0; vk < 256; ++vk) {
    SHORT s = GetAsyncKeyState(vk);
    g_keysNow[vk] = ((s & 0x8000) != 0) ? 1 : 0;
  }
}
bool RawInputPoll::IsKeyDown(int vk) {
  if (vk < 0 || vk > 255) {
    return false;
  }
  SHORT s = GetAsyncKeyState(vk);
  return (s & 0x8000) != 0;
}
bool RawInputPoll::WasKeyPressed(int vk) {
  if (vk < 0 || vk > 255) {
    return false;
  }
  return g_keysNow[vk] != 0 && g_keysPrev[vk] == 0;
}
bool RawInputPoll::WasKeyReleased(int vk) {
  if (vk < 0 || vk > 255) {
    return false;
  }
  return g_keysNow[vk] == 0 && g_keysPrev[vk] != 0;
}
bool RawInputPoll::IsMouseDown(int vk) {
  return IsKeyDown(vk);
}
void RawInputPoll::GetMousePosition(int& x, int& y) {
  POINT p;
  p.x = 0;
  p.y = 0;
  GetCursorPos(&p);
  x = p.x;
  y = p.y;
}
void RawInputPoll::GetMouseClientPosition(int& x, int& y) {
  POINT p;
  p.x = 0;
  p.y = 0;
  GetCursorPos(&p);
  if (g_hwnd != nullptr) {
    ScreenToClient(g_hwnd, &p);
  }
  x = p.x;
  y = p.y;
}
void RawInputPoll::GetMouseDelta(int& dx, int& dy) {
  dx = g_dx.exchange(0);
  dy = g_dy.exchange(0);
}
int RawInputPoll::GetMouseWheelDelta() {
  return g_wheel.exchange(0);
}
void RawInputPoll::OnRawMouseDelta(int dx, int dy) {
  g_dx.fetch_add(dx);
  g_dy.fetch_add(dy);
}
void RawInputPoll::OnMouseWheel(int delta) {
  g_wheel.fetch_add(delta);
}
bool RawInputPoll::IsGamepadConnected() {
  XINPUT_STATE st;
  std::memset(&st, 0, sizeof(st));
  return XInputGetState(0, &st) == ERROR_SUCCESS;
}
GamepadState RawInputPoll::GetGamepadState() {
  GamepadState out;
  XINPUT_STATE st;
  std::memset(&st, 0, sizeof(st));
  if (XInputGetState(0, &st) != ERROR_SUCCESS) {
    return out;
  }
  out.connected = true;
  out.buttons = static_cast<unsigned int>(st.Gamepad.wButtons);
  auto norm = [](short v, short dead) -> float {
    if (v > -dead && v < dead) {
      return 0.0f;
    }
    float f = static_cast<float>(v) / 32767.0f;
    if (f < -1.0f) {
      f = -1.0f;
    }
    if (f > 1.0f) {
      f = 1.0f;
    }
    return f;
  };
  out.leftX = norm(st.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  out.leftY = norm(st.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  out.rightX = norm(st.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  out.rightY = norm(st.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  out.leftTrigger = static_cast<float>(st.Gamepad.bLeftTrigger) / 255.0f;
  out.rightTrigger = static_cast<float>(st.Gamepad.bRightTrigger) / 255.0f;
  return out;
}
}
