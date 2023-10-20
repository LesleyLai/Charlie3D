#include "window_manager.hpp"
#include "window.hpp"

#include <SDL2/SDL.h>
#include <beyond/utils/panic.hpp>
#include <fmt/format.h>
#include <tracy/Tracy.hpp>

#ifdef _WIN32
#include <ShellScalingAPI.h>
#endif

namespace charlie {

auto WindowManager::instance() -> WindowManager&
{
  static WindowManager s;
  return s;
}

WindowManager::WindowManager()
{
  ZoneScoped;

#ifdef _WIN32
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    beyond::panic(fmt::format("Failed to initialize SDL: {}\n", SDL_GetError()));
  }
}

WindowManager::~WindowManager()
{
  ZoneScoped;
  SDL_Quit();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto WindowManager::create(int width, int height, beyond::ZStringView title,
                           const WindowOptions& options) -> Window
{
  uint32_t window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
  if (options.resizable) { window_flags |= SDL_WINDOW_RESIZABLE; }
  if (options.maximized) { window_flags |= SDL_WINDOW_MAXIMIZED; }

  auto* window = [&]() {
    ZoneScopedN("SDL_CreateWindow");
    return SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
                            height, window_flags);
  }();
  if (window == nullptr) {
    beyond::panic(fmt::format("Failed to create SDL window: {}\n", SDL_GetError()));
  }
  return Window{window};
}

} // namespace charlie