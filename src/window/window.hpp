#pragma once

#include <beyond/utils/copy_move.hpp>
#include <beyond/utils/utils.hpp>
#include <utility>

struct GLFWwindow;
class WindowManager;

struct Resolution {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

class Window {
  GLFWwindow* window_ = nullptr;

public:
  Window() = default;
  BEYOND_DELETE_COPY(Window)
  Window(Window&& other) noexcept : window_{std::exchange(other.window_, nullptr)} {}
  auto operator=(Window&& other) & noexcept -> Window&
  {
    if (this != &other) { window_ = std::exchange(other.window_, nullptr); }
    return *this;
  }
  ~Window();

  void swap_buffers() noexcept;

  [[nodiscard]] auto should_close() const noexcept -> bool;

  [[nodiscard]] BEYOND_FORCE_INLINE auto glfw_window() noexcept -> GLFWwindow*
  {
    return window_;
  }

  [[nodiscard]] auto resolution() const noexcept -> Resolution;

private:
  friend WindowManager;
  explicit Window(GLFWwindow* glfw_window) : window_{glfw_window} {}
};
