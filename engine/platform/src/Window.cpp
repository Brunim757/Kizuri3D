#include <caligo/core/Log.h>
#include <caligo/platform/Window.h>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <windows.h>
#include <windowsx.h>

namespace caligo::platform {

struct Window::Impl {
  HWND hwnd = nullptr;
  HINSTANCE instance = nullptr;
  Input input;
  HBRUSH background = nullptr;
  bool shouldClose = false;
  bool cursorVisible = true;
  uint32_t width = 0;
  uint32_t height = 0;
  float dpiScale = 1.0f;
};

namespace {

constexpr wchar_t kWindowClassName[] = L"CaligoWindowClass";

std::once_flag g_windowClassOnce;
bool g_windowClassRegistered = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void EnsureWindowClass(HINSTANCE instance) {
  std::call_once(g_windowClassOnce, [instance]() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = static_cast<UINT>(sizeof(windowClass));
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = &WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;
    g_windowClassRegistered =
        RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  });
}

std::wstring Utf8ToWide(std::string_view text, const wchar_t* fallback) {
  if (text.empty()) {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) {
    CALIGO_LOG_ERROR("window title is not valid UTF-8; using the fallback title");
    return fallback;
  }

  std::wstring result(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::optional<MouseButton> MouseButtonFromMessage(UINT message) noexcept {
  switch (message) {
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    return MouseButton::Left;
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    return MouseButton::Right;
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    return MouseButton::Middle;
  default:
    return std::nullopt;
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Window::Impl* impl = reinterpret_cast<Window::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
    impl = static_cast<Window::Impl*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
    impl->hwnd = hwnd;
  }

  if (impl == nullptr) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  switch (message) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    impl->shouldClose = true;
    PostQuitMessage(0);
    return 0;
  case WM_NCDESTROY:
    impl->hwnd = nullptr;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(0));
    return DefWindowProcW(hwnd, message, wParam, lParam);
  case WM_SIZE:
    impl->width = static_cast<uint32_t>(LOWORD(lParam));
    impl->height = static_cast<uint32_t>(HIWORD(lParam));
    return 0;
  case WM_DPICHANGED: {
    impl->dpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                 suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  case WM_ERASEBKGND: {
    if (impl->background != nullptr) {
      RECT client{};
      GetClientRect(hwnd, &client);
      FillRect(reinterpret_cast<HDC>(wParam), &client, impl->background);
    }
    return 1;
  }
  case WM_PAINT: {
    PAINTSTRUCT paint{};
    BeginPaint(hwnd, &paint);
    EndPaint(hwnd, &paint);
    return 0;
  }
  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT && !impl->cursorVisible) {
      SetCursor(nullptr);
      return TRUE;
    }
    break;
  case WM_INPUT: {
    RAWINPUT raw{};
    UINT size = static_cast<UINT>(sizeof(raw));
    const UINT read = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size,
                                      static_cast<UINT>(sizeof(RAWINPUTHEADER)));
    if (read != static_cast<UINT>(-1) && raw.header.dwType == RIM_TYPEMOUSE &&
        (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
      impl->input.ApplyMouseDelta(static_cast<float>(raw.data.mouse.lLastX),
                                  static_cast<float>(raw.data.mouse.lLastY));
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }
  case WM_MOUSEMOVE:
    impl->input.ApplyMouseMove(static_cast<float>(GET_X_LPARAM(lParam)),
                               static_cast<float>(GET_Y_LPARAM(lParam)));
    return 0;
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN: {
    const std::optional<MouseButton> button = MouseButtonFromMessage(message);
    if (button.has_value()) {
      impl->input.ApplyMouseButtonDown(*button);
      SetCapture(hwnd);
    }
    return 0;
  }
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP: {
    const std::optional<MouseButton> button = MouseButtonFromMessage(message);
    if (button.has_value()) {
      impl->input.ApplyMouseButtonUp(*button);
      ReleaseCapture();
    }
    return 0;
  }
  case WM_XBUTTONDOWN: {
    const WORD button = GET_XBUTTON_WPARAM(wParam);
    impl->input.ApplyMouseButtonDown(button == XBUTTON1 ? MouseButton::X1 : MouseButton::X2);
    SetCapture(hwnd);
    return TRUE;
  }
  case WM_XBUTTONUP: {
    const WORD button = GET_XBUTTON_WPARAM(wParam);
    impl->input.ApplyMouseButtonUp(button == XBUTTON1 ? MouseButton::X1 : MouseButton::X2);
    ReleaseCapture();
    return TRUE;
  }
  case WM_MOUSEWHEEL:
    impl->input.ApplyWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                           static_cast<float>(WHEEL_DELTA));
    return 0;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    const uint32_t scanCode = static_cast<uint32_t>((lParam >> 16) & 0xFF);
    const bool extended = (lParam & (1LL << 24)) != 0;
    const Key key = FromVirtualKey(static_cast<uint32_t>(wParam), scanCode, extended);
    if (key != Key::Unknown) {
      impl->input.ApplyKeyDown(key);
    }
    break;
  }
  case WM_KEYUP:
  case WM_SYSKEYUP: {
    const uint32_t scanCode = static_cast<uint32_t>((lParam >> 16) & 0xFF);
    const bool extended = (lParam & (1LL << 24)) != 0;
    const Key key = FromVirtualKey(static_cast<uint32_t>(wParam), scanCode, extended);
    if (key != Key::Unknown) {
      impl->input.ApplyKeyUp(key);
    }
    break;
  }
  case WM_KILLFOCUS:
    impl->input.Reset();
    ReleaseCapture();
    return 0;
  case WM_ACTIVATEAPP:
    if (wParam == FALSE) {
      impl->input.Reset();
      ReleaseCapture();
    }
    break;
  default:
    break;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

}

Window::Window() : m_impl(std::make_unique<Impl>()) {}

Window::~Window() {
  Destroy();
}

Result<void> Window::Create(const WindowDesc& desc) {
  if (m_impl->hwnd != nullptr) {
    return Error{"window is already open", 1};
  }
  if (desc.width == 0 || desc.height == 0) {
    return Error{"window size must be positive", 2};
  }

  m_impl->shouldClose = false;
  m_impl->instance = GetModuleHandleW(nullptr);
  EnsureWindowClass(m_impl->instance);
  if (!g_windowClassRegistered) {
    return Error{std::format("RegisterClassExW failed with error {}", GetLastError()), 3};
  }

  m_impl->width = desc.width;
  m_impl->height = desc.height;
  m_impl->background = CreateSolidBrush(RGB(18, 18, 20));

  const std::wstring title = Utf8ToWide(desc.title, L"Caligo");
  DWORD style = static_cast<DWORD>(WS_OVERLAPPEDWINDOW);
  if (!desc.resizable) {
    style &= ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }

  RECT rect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
  const UINT systemDpi = GetDpiForSystem();
  if (AdjustWindowRectExForDpi(&rect, style, FALSE, 0, systemDpi) == FALSE) {
    AdjustWindowRectEx(&rect, style, FALSE, 0);
  }

  const HWND hwnd = CreateWindowExW(0, kWindowClassName, title.c_str(), style, CW_USEDEFAULT,
                                    CW_USEDEFAULT, static_cast<int>(rect.right - rect.left),
                                    static_cast<int>(rect.bottom - rect.top), nullptr, nullptr,
                                    m_impl->instance, m_impl.get());
  if (hwnd == nullptr) {
    const DWORD error = GetLastError();
    Destroy();
    return Error{std::format("CreateWindowExW failed with error {}", error), 4};
  }

  m_impl->hwnd = hwnd;
  m_impl->dpiScale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;

  RAWINPUTDEVICE rawInput{};
  rawInput.usUsagePage = 0x01;
  rawInput.usUsage = 0x02;
  rawInput.dwFlags = 0;
  rawInput.hwndTarget = hwnd;
  if (RegisterRawInputDevices(&rawInput, 1, static_cast<UINT>(sizeof(rawInput))) == FALSE) {
    const DWORD error = GetLastError();
    Destroy();
    return Error{std::format("RegisterRawInputDevices failed with error {}", error), 5};
  }

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  return {};
}

void Window::Destroy() noexcept {
  if (m_impl->hwnd != nullptr) {
    RAWINPUTDEVICE rawInput{};
    rawInput.usUsagePage = 0x01;
    rawInput.usUsage = 0x02;
    rawInput.dwFlags = RIDEV_REMOVE;
    rawInput.hwndTarget = nullptr;
    RegisterRawInputDevices(&rawInput, 1, static_cast<UINT>(sizeof(rawInput)));

    HWND hwnd = m_impl->hwnd;
    m_impl->hwnd = nullptr;
    DestroyWindow(hwnd);
  }

  if (m_impl->background != nullptr) {
    DeleteObject(m_impl->background);
    m_impl->background = nullptr;
  }

  m_impl->input.Reset();
  m_impl->shouldClose = true;
}

void Window::PollEvents() {
  if (m_impl->hwnd == nullptr) {
    return;
  }

  m_impl->input.NewFrame();
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
    if (message.message == WM_QUIT) {
      m_impl->shouldClose = true;
      break;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
}

void Window::RequestClose() noexcept {
  if (m_impl->hwnd != nullptr) {
    PostMessageW(m_impl->hwnd, WM_CLOSE, 0, 0);
  }
}

bool Window::IsOpen() const noexcept {
  return m_impl->hwnd != nullptr;
}

bool Window::ShouldClose() const noexcept {
  return m_impl->shouldClose;
}

void* Window::NativeHandle() const noexcept {
  return m_impl->hwnd;
}

uint32_t Window::Width() const noexcept {
  return m_impl->width;
}

uint32_t Window::Height() const noexcept {
  return m_impl->height;
}

float Window::DpiScale() const noexcept {
  return m_impl->dpiScale;
}

Input& Window::GetInput() noexcept {
  return m_impl->input;
}

const Input& Window::GetInput() const noexcept {
  return m_impl->input;
}

void Window::SetTitle(std::string_view title) {
  if (m_impl->hwnd == nullptr) {
    return;
  }
  const std::wstring wide = Utf8ToWide(title, L"Caligo");
  SetWindowTextW(m_impl->hwnd, wide.c_str());
}

void Window::SetCursorVisible(bool visible) noexcept {
  if (m_impl->cursorVisible == visible) {
    return;
  }
  m_impl->cursorVisible = visible;
  if (m_impl->hwnd != nullptr) {
    SetCursor(visible ? LoadCursorW(nullptr, IDC_ARROW) : nullptr);
  }
}

}
