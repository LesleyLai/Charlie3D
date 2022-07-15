#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/renderer.hpp"

#include <beyond/math/angle.hpp>
#include <beyond/math/transform.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void init_load_empire_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;
  using beyond::Mat4;

  charlie::Mesh* lost_empire_mesh = renderer.get_mesh("lost_empire");
  BEYOND_ENSURE(lost_empire_mesh != nullptr);
  charlie::Material* default_mat = renderer.get_material("default");
  BEYOND_ENSURE(default_mat != nullptr);
  renderer.add_object(charlie::RenderObject{
      .mesh = lost_empire_mesh,
      .material = default_mat,
      .model_matrix = beyond::translate(5.f, -10.f, 0.f)});
}

void show_gui()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow();

  bool show_gui = true;
  ImGui::Begin("Control Panel", &show_gui);

  ImGui::Text("Scenes:");
  const char* items[] = {"Lost Empire", "PBR Demo: Sphere"};
  static int item_current_idx = 0;
  if (ImGui::BeginListBox(
          "##Scenes listbox",
          ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing()))) {
    for (int n = 0; n < IM_ARRAYSIZE(items); n++) {
      const bool is_selected = (item_current_idx == n);
      if (ImGui::Selectable(items[n], is_selected)) item_current_idx = n;

      // Set the initial focus when opening the combo (scrolling + keyboard
      // navigation focus)
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndListBox();
  }

  ImGui::End();
}

auto main() -> int
{
  auto& window_manager = WindowManager::instance();
  Window window = window_manager.create(1024, 768, "Charlie3D");
  charlie::Renderer renderer{window};

  init_load_empire_scene(renderer);

  while (!window.should_close()) {
    window_manager.pull_events();
    window.swap_buffers();

    show_gui();

    renderer.render();
  }
}