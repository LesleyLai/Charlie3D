#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/renderer.hpp"

void init_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;
  using beyond::Mat4;

  charlie::Mesh* bunny_mesh = renderer.get_mesh("bunny");
  BEYOND_ENSURE(bunny_mesh != nullptr);
  charlie::Material* default_mat = renderer.get_material("default");
  BEYOND_ENSURE(default_mat != nullptr);
  const Mat4 bunny_transform =
      beyond::translate(0.f, -0.5f, 0.f) *
      beyond::rotate_y(
          beyond::Degree{static_cast<float>(renderer.frame_number()) * 0.2f}) *
      beyond::rotate_x(-90._deg);
  renderer.add_object(
      charlie::RenderObject{.mesh = bunny_mesh,
                            .material = default_mat,
                            .transform_matrix = bunny_transform});

  charlie::Mesh* triangle_mesh = renderer.get_mesh("triangle");
  BEYOND_ENSURE(triangle_mesh != nullptr);

  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {
      const Mat4 translation =
          beyond::translate(static_cast<float>(x), 0.f, static_cast<float>(y));
      constexpr Mat4 scale = beyond::scale(0.2f, 0.2f, 0.2f);
      renderer.add_object(
          charlie::RenderObject{.mesh = triangle_mesh,
                                .material = default_mat,
                                .transform_matrix = translation * scale});
    }
  }
}

auto main() -> int
{
  auto& window_manager = WindowManager::instance();
  Window window = window_manager.create(1024, 768, "Charlie3D");
  charlie::Renderer renderer{window};

  init_scene(renderer);

  while (!window.should_close()) {
    window_manager.pull_events();
    window.swap_buffers();

    renderer.render();
  }
}