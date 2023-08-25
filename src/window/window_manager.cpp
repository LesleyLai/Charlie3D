#include "window_manager.hpp"
#include "window.hpp"

#include <SDL2/SDL.h>

#include <fmt/format.h>

#include <beyond/utils/panic.hpp>

namespace charlie {

auto WindowManager::instance() -> WindowManager&
{
  static WindowManager s;
  return s;
}

WindowManager::WindowManager()
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    beyond::panic("Failed to initialize SDL: {}\n", SDL_GetError());
  }
}

WindowManager::~WindowManager()
{
  SDL_Quit();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto WindowManager::create(int width, int height, const char* title, const WindowOptions& options)
    -> Window
{
  uint32_t window_flags = SDL_WINDOW_VULKAN;
  if (options.resizable) { window_flags |= SDL_WINDOW_RESIZABLE; }

  auto* window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
                                  height, window_flags);
  if (window == nullptr) { beyond::panic("Failed to create SDL window: {}\n", SDL_GetError()); }
  return Window{window};
}

} // namespace charlie