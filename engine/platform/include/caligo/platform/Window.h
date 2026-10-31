#pragma once

#include <caligo/core/Result.h>
#include <caligo/platform/Input.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace caligo::platform {

struct WindowDesc {
  std::string title = "Caligo";
  uint32_t width = 1280;
  uint32_t height = 720;
  bool resizable = true;
};

class Window final {
public:
  struct Impl;

  Window();
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  [[nodiscard]] Result<void> Create(const WindowDesc& desc);
  void Destroy() noexcept;
  void PollEvents();
  void RequestClose() noexcept;

  [[nodiscard]] bool IsOpen() const noexcept;
  [[nodiscard]] bool ShouldClose() const noexcept;
  [[nodiscard]] void* NativeHandle() const noexcept;
  [[nodiscard]] uint32_t Width() const noexcept;
  [[nodiscard]] uint32_t Height() const noexcept;
  [[nodiscard]] float DpiScale() const noexcept;

  [[nodiscard]] Input& GetInput() noexcept;
  [[nodiscard]] const Input& GetInput() const noexcept;

  void SetTitle(std::string_view title);
  void SetCursorVisible(bool visible) noexcept;

private:
  std::unique_ptr<Impl> m_impl;
};

}
