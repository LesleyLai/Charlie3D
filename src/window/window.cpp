#include "window.hpp"

#include <SDL2/SDL.h>

namespace charlie {

Window::~Window()
{
  SDL_DestroyWindow(window_);
}

[[nodiscard]] auto Window::resolution() const noexcept -> Resolution
{
  int width = 0, height = 0;
  SDL_GetWindowSize(window_, &width, &height);
  return Resolution{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

[[nodiscard]] auto Window::window_id() const noexcept -> std::uint32_t
{
  return SDL_GetWindowID(window_);
}

} // namespace charlie