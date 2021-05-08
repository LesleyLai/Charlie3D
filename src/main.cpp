#include "window/window.hpp"
#include "window/window_manager.hpp"

auto main() -> int
{
  auto& window_manager = WindowManager::instance();
  Window window = window_manager.create(1024, 768, "Charlie3D");

  while (!window.should_close()) {
    window_manager.pull_events();
    window.swap_buffers();
  }
}