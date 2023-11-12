#pragma once

#include "resolution.hpp"
#include <beyond/utils/copy_move.hpp>
#include <beyond/utils/utils.hpp>
#include <utility>

struct SDL_Window;

namespace charlie {

class WindowManager;

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

  [[nodiscard]] BEYOND_FORCE_INLINE auto raw_window() noexcept -> SDL_Window* { return window_; }

  [[nodiscard]] auto resolution() const noexcept -> Resolution;

  [[nodiscard]] auto window_id() const noexcept -> u32;

private:
  friend WindowManager;
  explicit Window(SDL_Window* raw_window) : window_{raw_window} {}
};

} // namespace charlie