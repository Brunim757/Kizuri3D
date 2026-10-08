#pragma once
#include <functional>
namespace Kizuri {
class Window {
public:
  using MessageHook = std::function<long long(void* hwnd, unsigned int msg, unsigned long long wParam, long long lParam)>;
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
  void SetMessageHook(MessageHook hook);
  void ClearMessageHook();
private:
  void* hwnd;
  unsigned long long instance;
  int width;
  int height;
  bool open;
  bool created;
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
};
}
