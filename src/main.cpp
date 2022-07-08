#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/renderer.hpp"

#include <beyond/math/angle.hpp>
#include <beyond/math/transform.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void init_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;
  using beyond::Mat4;

  charlie::Mesh* lost_empire_mesh = renderer.get_mesh("lost_empire");
  BEYOND_ENSURE(lost_empire_mesh != nullptr);
  charlie::Material* default_mat = renderer.get_material("default");
  BEYOND_ENSURE(default_mat != nullptr);
  //  const Mat4 bunny_transform =
  //      beyond::translate(0.f, -0.5f, 0.f) *
  //      beyond::rotate_y(
  //          beyond::Degree{static_cast<float>(renderer.frame_number()) *
  //          0.2f}) *
  //      beyond::rotate_x(-90._deg);
  renderer.add_object(charlie::RenderObject{
      .mesh = lost_empire_mesh,
      .material = default_mat,
      .model_matrix = beyond::translate(5.f, -10.f, 0.f)});
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

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    renderer.render();
  }
}