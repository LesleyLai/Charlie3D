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

#include <tracy/Tracy.hpp>

#include <chrono>

void init_lost_empire_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;

  const auto cpu_mesh = charlie::CPUMesh::load("lost_empire/lost_empire.obj");

  const charlie::MeshHandle lost_empire_mesh = renderer.upload_mesh_data("lost_empire", cpu_mesh);

  const charlie::Material* default_material = renderer.get_material("default");
  BEYOND_ENSURE(default_material != nullptr);
  renderer.add_object(charlie::RenderObject{.mesh = lost_empire_mesh,
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

    glfwSetWindowUserPointer(window.glfw_window(), this);

    glfwSetWindowSizeCallback(
        window.glfw_window(), [](GLFWwindow* glfw_window, int width_, int height_) {
          fmt::print("Window resizes to {}x{}!\n", width_, height_);

          auto& app = *static_cast<App*>(glfwGetWindowUserPointer(glfw_window));
          app.renderer.resize({.width = beyond::to_u32(width_), .height = beyond::to_u32(height_)});

          app.camera.aspect_ratio = static_cast<float>(width_) / static_cast<float>(height_);
        });
    glfwSetCursorPosCallback(
        window.glfw_window(), [](GLFWwindow* glfw_window, double xpos, double ypos) {
          ImGui_ImplGlfw_CursorPosCallback(glfw_window, xpos, ypos);

          const ImGuiIO& io = ImGui::GetIO();
          if (!io.WantCaptureMouse) {
            auto& app = *static_cast<App*>(glfwGetWindowUserPointer(glfw_window));
            app.camera.on_mouse_move(glfw_window, static_cast<int>(xpos), static_cast<int>(ypos));
          }
        });
    glfwSetScrollCallback(
        window.glfw_window(), [](GLFWwindow* glfw_window, double xoffset, double yoffset) {
          ImGui_ImplGlfw_ScrollCallback(glfw_window, xoffset, yoffset);

          const ImGuiIO& io = ImGui::GetIO();
          if (!io.WantCaptureMouse) {
            auto& app = *static_cast<App*>(glfwGetWindowUserPointer(glfw_window));
            app.camera.on_mouse_scroll(static_cast<float>(xoffset), static_cast<float>(yoffset));
          }
        });
    glfwSetKeyCallback(window.glfw_window(),
                       [](GLFWwindow* glfw_window, int key, int scancode, int action, int mods) {
                         ImGui_ImplGlfw_KeyCallback(glfw_window, key, scancode, action, mods);

                         const ImGuiIO& io = ImGui::GetIO();
                         if (!io.WantCaptureKeyboard) {
                           auto& app = *static_cast<App*>(glfwGetWindowUserPointer(glfw_window));
                           app.camera.on_key_input(key, scancode, action, mods);
                         }
                       });
  }

  void update()
  {
    camera.update();
  }

  void run()
  {
    using namespace std::literals::chrono_literals;
    auto previous_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration lag = 0ns;
    static constexpr auto MS_PER_UPDATE = 10ms;

    while (!window.should_close()) {
      const auto current_time = std::chrono::steady_clock::now();
      const auto delta_time = current_time - previous_time;
      previous_time = current_time;
      lag += delta_time;

      charlie::WindowManager::instance().pull_events();
      window.swap_buffers();

      draw_gui();

      while (lag >= MS_PER_UPDATE) {
        update();
        lag -= MS_PER_UPDATE;
      }

      renderer.render(camera);

      FrameMark;
    }
  }

private:
  void draw_gui()
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();

    bool show_control_panel = true;
    const auto resolution = window.resolution();
    const float control_panel_width = ImGui::GetFontSize() * 25.f;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(resolution.width) - control_panel_width, 0));
    ImGui::SetNextWindowSize(ImVec2(control_panel_width, static_cast<float>(resolution.height)));
    ImGui::Begin("Control Panel", &show_control_panel, ImGuiWindowFlags_NoDecoration);

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) { camera.draw_gui(); }

    ImGui::End();
  }
};

void set_asset_path()
{
  const auto current_path = std::filesystem::current_path();
  auto asset_path = charlie::locate_asset_path().expect("Cannot find assets folder!");

  Configurations::instance().set(CONFIG_ASSETS_PATH, asset_path);
}

auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  set_asset_path();

  App app;

  init_lost_empire_scene(app.renderer);

  app.run();
}