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
  return Resolution{beyond::narrow<std::uint32_t>(width), beyond::narrow<std::uint32_t>(height)};
}

[[nodiscard]] auto Window::window_id() const noexcept -> std::uint32_t
{
  return SDL_GetWindowID(window_);
}

} // namespace charlie