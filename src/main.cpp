#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "utils/configuration.hpp"
#include "utils/file.hpp"

#include <beyond/math/angle.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>
#include <utility>

void init_lost_empire_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;

  const auto cpu_mesh = charlie::CPUMesh::load("lost_empire/lost_empire.obj");

  const charlie::Mesh& lost_empire_mesh = renderer.upload_mesh_data("lost_empire", cpu_mesh);

  const charlie::Material* default_material = renderer.get_material("default");
  BEYOND_ENSURE(default_material != nullptr);
  renderer.add_object(charlie::RenderObject{.mesh = &lost_empire_mesh,
                                            .material = default_material,
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
  if (ImGui::BeginListBox("##Scenes listbox",
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

struct App {
  charlie::Window window;
  charlie::Renderer renderer;
  charlie::Camera camera;

  App()
      : window{charlie::WindowManager::instance().create(1920, 1080, "Charlie3D",
                                                         {
                                                             .resizable = true,
                                                         })},
        renderer{window}
  {
    const auto [width, height] = window.resolution();
    camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  }

  void run()
  {
    while (!window.should_close()) {
      charlie::WindowManager::instance().pull_events();
      window.swap_buffers();

      camera.update();

      show_gui(camera);

      renderer.render(camera);
    }
  }
};

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto& camera = static_cast<App*>(glfwGetWindowUserPointer(window))->camera;
  camera.process_key_input(key, scancode, action, mods);
}

void resize_callback(GLFWwindow* window, int width, int height)
{
  fmt::print("Window resizes to {}x{}!\n", width, height);
  std::fflush(stdout);

  auto& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
  app.renderer.resize({.width = beyond::to_u32(width), .height = beyond::to_u32(height)});

  app.camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
}

void set_asset_path()
{
  const auto current_path = std::filesystem::current_path();
  auto asset_path = charlie::locate_asset_path(current_path).expect("Cannot find assets folder!");

  Configurations::instance().set(CONFIG_ASSETS_PATH, asset_path);
}

auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  set_asset_path();

  App app;
  glfwSetWindowUserPointer(app.window.glfw_window(), &app);
  glfwSetWindowSizeCallback(app.window.glfw_window(), resize_callback);
  glfwSetKeyCallback(app.window.glfw_window(), key_callback);

  init_lost_empire_scene(app.renderer);

  app.run();
}