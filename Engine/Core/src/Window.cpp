#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Window.h"
#include <windows.h>
namespace Kizuri {
namespace {
const wchar_t* kClassName = L"KizuriEngineWindow";
Window::MessageHook g_hook;
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (g_hook) {
    long long handled = g_hook(hWnd, msg, static_cast<unsigned long long>(wParam), static_cast<long long>(lParam));
    if (handled != 0) {
      return static_cast<LRESULT>(handled);
    }
  }
  Window* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<Window*>(cs->lpCreateParams);
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
  }
  switch (msg) {
    case WM_CLOSE:
      DestroyWindow(hWnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_SIZE: {
      if (self != nullptr) {
        int w = static_cast<int>(LOWORD(lParam));
        int h = static_cast<int>(HIWORD(lParam));
        (void)w;
        (void)h;
      }
      return 0;
    }
    case WM_INPUT: {
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}
}
Window::Window()
  : hwnd(nullptr)
  , instance(0)
  , width(0)
  , height(0)
  , open(false)
  , created(false) {
}
Window::~Window() {
  Destroy();
}
bool Window::Create(const wchar_t* title, int w, int h) {
  if (created) {
    return true;
  }
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
  RegisterClassExW(&wc);
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
  instance = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hInst));
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
  UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
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
void Window::SetMessageHook(MessageHook hook) {
  g_hook = hook;
}
void Window::ClearMessageHook() {
  g_hook = nullptr;
}
}
