#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Window.h"
#include "Kizuri/Input.h"
#include <windows.h>
namespace Kizuri {
namespace {
const wchar_t* kClassName = L"Kizuri3DWindow";
void EnableDpiAwareness() {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32 != nullptr) {
    typedef int (WINAPI* SetCtxFn)(int);
    SetCtxFn setCtx = reinterpret_cast<SetCtxFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setCtx != nullptr) {
      if (setCtx(-4) != 0) {
        return;
      }
    }
  }
  SetProcessDPIAware();
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  Window* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<Window*>(cs->lpCreateParams);
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
  }
  if (msg == WM_INPUT) {
    UINT rawSize = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &rawSize, sizeof(RAWINPUTHEADER));
    if (rawSize > 0 && rawSize < 1024) {
      unsigned char buf[1024];
      UINT readSize = rawSize;
      if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buf, &readSize, sizeof(RAWINPUTHEADER)) == rawSize) {
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf);
        if (raw->header.dwType == RIM_TYPEMOUSE) {
          int dx = raw->data.mouse.lLastX;
          int dy = raw->data.mouse.lLastY;
          if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 && (dx != 0 || dy != 0)) {
            RawInputPoll::OnRawMouseDelta(dx, dy);
          }
        }
      }
    }
    return 0;
  }
  if (self != nullptr && self->HandleMessage(hWnd, msg, wParam, lParam)) {
    return 0;
  }
  switch (msg) {
    case WM_CLOSE:
      DestroyWindow(hWnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}
}
Window::Window()
  : hwnd(nullptr)
  , width(0)
  , height(0)
  , open(false)
  , created(false)
  , fullscreen(false)
  , savedStyle(0)
  , savedX(0)
  , savedY(0)
  , savedW(0)
  , savedH(0) {
}
Window::~Window() {
  Destroy();
}
bool Window::HandleMessage(void* h, unsigned int msg, unsigned long long wParam, long long lParam) {
  if (hook) {
    long long handled = hook(h, msg, wParam, lParam);
    if (handled != 0) {
      return true;
    }
  }
  if (msg == WM_SIZE) {
    int w = static_cast<int>(LOWORD(lParam));
    int h = static_cast<int>(HIWORD(lParam));
    if (w > 0 && h > 0 && (w != width || h != height)) {
      width = w;
      height = h;
      if (onResize) {
        onResize(w, h);
      }
    }
    return true;
  }
  return false;
}
bool Window::Create(const wchar_t* title, int w, int h) {
  if (created) {
    return true;
  }
  EnableDpiAwareness();
  HINSTANCE hInst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc;
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInst;
  wc.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
  wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = kClassName;
  wc.hIconSm = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
  if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }
  DWORD style = WS_OVERLAPPEDWINDOW;
  RECT rc;
  rc.left = 0;
  rc.top = 0;
  rc.right = w;
  rc.bottom = h;
  AdjustWindowRect(&rc, style, FALSE);
  HWND handle = CreateWindowExW(
    0,
    kClassName,
    title,
    style,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    rc.right - rc.left,
    rc.bottom - rc.top,
    nullptr,
    nullptr,
    hInst,
    this);
  if (handle == nullptr) {
    return false;
  }
  hwnd = handle;
  width = w;
  height = h;
  open = true;
  created = true;
  ShowWindow(static_cast<HWND>(hwnd), SW_SHOW);
  UpdateWindow(static_cast<HWND>(hwnd));
  return true;
}
void Window::Destroy() {
  if (!created) {
    return;
  }
  HWND handle = static_cast<HWND>(hwnd);
  if (handle != nullptr) {
    DestroyWindow(handle);
  }
  hwnd = nullptr;
  open = false;
  created = false;
  fullscreen = false;
  hook = nullptr;
  onResize = nullptr;
}
bool Window::IsOpen() const {
  return open && created;
}
bool Window::IsMinimized() const {
  HWND handle = static_cast<HWND>(hwnd);
  if (handle == nullptr) {
    return false;
  }
  return IsIconic(handle) != FALSE;
}
bool Window::PollEvents() {
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      open = false;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return open;
}
void* Window::NativeHandle() const {
  return hwnd;
}
int Window::Width() const {
  return width;
}
int Window::Height() const {
  return height;
}
void Window::ClientSize(int& w, int& h) const {
  HWND handle = static_cast<HWND>(hwnd);
  if (handle == nullptr) {
    w = width;
    h = height;
    return;
  }
  RECT rc;
  if (GetClientRect(handle, &rc) == FALSE) {
    w = width;
    h = height;
    return;
  }
  w = static_cast<int>(rc.right - rc.left);
  h = static_cast<int>(rc.bottom - rc.top);
}
void Window::SetMessageHook(MessageHook h) {
  hook = h;
}
void Window::ClearMessageHook() {
  hook = nullptr;
}
void Window::SetResizeCallback(ResizeCallback cb) {
  onResize = cb;
}
bool Window::SetFullscreen(bool fs) {
  HWND handle = static_cast<HWND>(hwnd);
  if (handle == nullptr) {
    return false;
  }
  if (fs == fullscreen) {
    return true;
  }
  if (fs) {
    savedStyle = static_cast<unsigned long>(GetWindowLongW(handle, GWL_STYLE));
    RECT rc;
    if (GetWindowRect(handle, &rc) == FALSE) {
      return false;
    }
    savedX = static_cast<int>(rc.left);
    savedY = static_cast<int>(rc.top);
    savedW = static_cast<int>(rc.right - rc.left);
    savedH = static_cast<int>(rc.bottom - rc.top);
    SetWindowLongW(handle, GWL_STYLE, static_cast<LONG>(savedStyle & ~(WS_CAPTION | WS_THICKFRAME)));
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTOPRIMARY), &mi) == FALSE) {
      return false;
    }
    SetWindowPos(handle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    fullscreen = true;
    return true;
  }
  SetWindowLongW(handle, GWL_STYLE, static_cast<LONG>(savedStyle));
  SetWindowPos(handle, HWND_NOTOPMOST, savedX, savedY, savedW, savedH, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  fullscreen = false;
  return true;
}
bool Window::IsFullscreen() const {
  return fullscreen;
}
}
