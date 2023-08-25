#pragma once

#include <beyond/utils/copy_move.hpp>
#include <beyond/utils/utils.hpp>
#include <utility>

struct SDL_Window;

namespace charlie {

class WindowManager;

struct Resolution {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

class Window {
  SDL_Window* window_ = nullptr;

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

  [[nodiscard]] BEYOND_FORCE_INLINE auto raw_window() noexcept -> SDL_Window*
  {
    return window_;
  }

  [[nodiscard]] auto resolution() const noexcept -> Resolution;

private:
  friend WindowManager;
  explicit Window(SDL_Window* raw_window) : window_{raw_window} {}
};

} // namespace charlie