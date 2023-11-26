#include "window.hpp"

#include <SDL2/SDL.h>

#include "beyond/utils/narrowing.hpp"

namespace charlie {

Window::~Window()
{
  SDL_DestroyWindow(window_);
}

[[nodiscard]] auto Window::resolution() const noexcept -> Resolution
{
  int width = 0, height = 0;
  SDL_GetWindowSize(window_, &width, &height);
  return Resolution{beyond::narrow<u32>(width), beyond::narrow<u32>(height)};
}

[[nodiscard]] auto Window::window_id() const noexcept -> u32
{
  return SDL_GetWindowID(window_);
}

auto Window::is_minimized() const noexcept -> bool
{
  return SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED;
}

} // namespace charlie