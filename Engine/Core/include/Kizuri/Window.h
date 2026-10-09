#pragma once
#include <functional>
namespace Kizuri {
class Window {
public:
  using MessageHook = std::function<long long(void* hwnd, unsigned int msg, unsigned long long wParam, long long lParam)>;
  using ResizeCallback = std::function<void(int width, int height)>;
  Window();
  ~Window();
  bool Create(const wchar_t* title, int width, int height);
  void Destroy();
  bool IsOpen() const;
  bool IsMinimized() const;
  bool PollEvents();
  void* NativeHandle() const;
  int Width() const;
  int Height() const;
  void ClientSize(int& width, int& height) const;
  void SetMessageHook(MessageHook hook);
  void ClearMessageHook();
  void SetResizeCallback(ResizeCallback cb);
  bool SetFullscreen(bool fullscreen);
  bool IsFullscreen() const;
  bool HandleMessage(void* h, unsigned int msg, unsigned long long wParam, long long lParam);
private:
  void* hwnd;
  int width;
  int height;
  bool open;
  bool created;
  bool fullscreen;
  unsigned long savedStyle;
  int savedX;
  int savedY;
  int savedW;
  int savedH;
  MessageHook hook;
  ResizeCallback onResize;
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
};
}
