#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include <beyond/math/angle.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <filesystem>
#include <utility>

[[nodiscard]] auto locate_asset_folder()
{
  // std::filesystem::
}

void key_callback(GLFWwindow* window, int key, int scancode, int action,
                  int mods)
{
  auto* camera =
      static_cast<charlie::Camera*>(glfwGetWindowUserPointer(window));
  camera->process_key_input(key, scancode, action, mods);
}

void init_lost_empire_scene(charlie::Renderer& renderer,
                            std::filesystem::path assets_path)
{
  using namespace beyond::literals;
  using beyond::Mat4;

  const charlie::Mesh& lost_empire_mesh = renderer.upload_mesh(
      "lost_empire",
      (assets_path / "lost_empire/lost_empire.obj").string().c_str());

  const charlie::Material* default_mat = renderer.get_material("default");
  BEYOND_ENSURE(default_mat != nullptr);
  renderer.add_object(charlie::RenderObject{
      .mesh = &lost_empire_mesh,
      .material = default_mat,
      .model_matrix = beyond::translate(0.f, -20.f, 0.f)});
}

void show_gui(charlie::Camera& camera)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow();

  bool show_gui = true;
  ImGui::Begin("Control Panel", &show_gui);

  ImGui::Text("Scenes:");
  const char* items[] = {"Lost Empire", "Sponza"};
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

  ImGui::Text("Camera:");
  ImGui::InputFloat3("position", camera.position.elem);

  ImGui::End();
}

template <typename Fn>
requires std::is_invocable_r_v<bool, Fn, std::filesystem::path>
auto upward_directory_find(const std::filesystem::path& from, Fn condition)
    -> beyond::optional<std::filesystem::path>
{
  for (auto directory_path = from; directory_path != from.root_path();
       directory_path = directory_path.parent_path()) {
    if (condition(directory_path)) { return directory_path; }
  }
  return beyond::nullopt;
}

auto locate_asset_path(const std::filesystem::path& exe_directory_path)
{
  using std::filesystem::path;
  const auto append_asset = [](const path& path) { return path / "assets"; };
  const auto parent_path = upward_directory_find(
      exe_directory_path, [&](const std::filesystem::path& path) {
        const auto assets_path = append_asset(path);
        return exists(assets_path) && is_directory(assets_path);
      });
  return parent_path.map([&](const path& path) { return append_asset(path); });
}

auto main(int /*argc*/, char* argv[]) -> int
{
  const auto exe_directory_path =
      std::filesystem::path{argv[0]}.remove_filename();

  const auto asset_path = locate_asset_path(exe_directory_path)
                              .expect("Cannot find assets folder!");

  auto& window_manager = WindowManager::instance();
  Window window = window_manager.create(1024, 768, "Charlie3D");
  charlie::Renderer renderer{window};

  charlie::Camera camera;
  glfwSetWindowUserPointer(window.glfw_window(), &camera);
  glfwSetKeyCallback(window.glfw_window(), key_callback);

  init_lost_empire_scene(renderer, asset_path);

  while (!window.should_close()) {
    window_manager.pull_events();
    window.swap_buffers();

    camera.update();

    show_gui(camera);

    renderer.render(camera);
  }
}