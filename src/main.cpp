#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "utils/configuration.hpp"
#include "utils/file.hpp"

#include <beyond/math/transform.hpp>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>
#include <utility>

#include <tracy/Tracy.hpp>

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

struct App {
  charlie::Window window;
  charlie::Renderer renderer;
  charlie::FirstPersonCameraController first_person_controller;
  charlie::ArcballCameraController arcball_controller;
  charlie::Camera camera;

  App()
      : window{charlie::WindowManager::instance().create(1920, 1080, "Charlie3D",
                                                         {
                                                             .resizable = true,
                                                         })},
        renderer{window}, camera{arcball_controller}
  {

    const auto [width, height] = window.resolution();
    camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  }

  void draw_gui()
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();

    bool show_control_panel = true;
    const auto resolution = window.resolution();
    const float control_panel_width = ImGui::GetFontSize() * 20.f;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(resolution.width) - control_panel_width, 0));
    ImGui::SetNextWindowSize(ImVec2(control_panel_width, static_cast<float>(resolution.height)));
    ImGui::Begin("Control Panel", &show_control_panel, ImGuiWindowFlags_NoDecoration);

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) { camera.draw_gui(); }

    ImGui::End();
  }

  void run()
  {
    while (!window.should_close()) {
      charlie::WindowManager::instance().pull_events();
      window.swap_buffers();

      camera.update();

      draw_gui();

      renderer.render(camera);

      FrameMark;
    }
  }
};

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto& camera = static_cast<App*>(glfwGetWindowUserPointer(window))->camera;
  camera.on_key_input(key, scancode, action, mods);
}

void resize_callback(GLFWwindow* window, int width, int height)
{
  fmt::print("Window resizes to {}x{}!\n", width, height);

  auto& app = *static_cast<App*>(glfwGetWindowUserPointer(window));
  app.renderer.resize({.width = beyond::to_u32(width), .height = beyond::to_u32(height)});

  app.camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
}

void cursor_callback(GLFWwindow* window, double xpos, double ypos)
{
  ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

  const ImGuiIO& io = ImGui::GetIO();
  if (!io.WantCaptureMouse) {
    auto& camera = static_cast<App*>(glfwGetWindowUserPointer(window))->camera;
    camera.on_mouse_move(window, static_cast<int>(xpos), static_cast<int>(ypos));
  }
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
  glfwSetCursorPosCallback(app.window.glfw_window(), cursor_callback);
  glfwSetKeyCallback(app.window.glfw_window(), key_callback);

  init_lost_empire_scene(app.renderer);

  app.run();
}