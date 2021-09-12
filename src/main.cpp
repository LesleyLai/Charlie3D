#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/renderer.hpp"

auto main() -> int
{
  auto& window_manager = WindowManager::instance();
  Window window = window_manager.create(1024, 768, "Charlie3D");
  charlie::Renderer renderer{window};

  while (!window.should_close()) {
    window_manager.pull_events();
    window.swap_buffers();

    renderer.render();
  }
}