#include "window/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "utils/configuration.hpp"
#include "utils/file.hpp"

#include <beyond/math/transform.hpp>

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <SDL2/SDL.h>

#include <tracy/Tracy.hpp>

#include <chrono>

#ifdef _WIN32
#include <ShellScalingAPI.h>
#endif

void init_lost_empire_scene(charlie::Renderer& renderer)
{
  using namespace beyond::literals;

  const auto cpu_mesh = charlie::CPUMesh::load("models/lost_empire/lost_empire.obj");

  const charlie::MeshHandle lost_empire_mesh = renderer.upload_mesh_data("lost_empire", cpu_mesh);

  const charlie::Material* default_material = renderer.get_material("default");
  BEYOND_ENSURE(default_material != nullptr);
  renderer.add_object(charlie::RenderObject{.mesh = lost_empire_mesh,
                                            .material = default_material,
                                            .model_matrix = beyond::translate(0.f, -20.f, 0.f)});
}

void set_asset_path()
{
  const auto current_path = std::filesystem::current_path();
  auto asset_path = charlie::locate_asset_path().expect("Cannot find assets folder!");

  Configurations::instance().set(CONFIG_ASSETS_PATH, asset_path);
}

void draw_gui(charlie::Window& window, charlie::Camera& camera)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
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

int main()
{
#ifdef _WIN32
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

  set_asset_path();

  auto& window_manager = charlie::WindowManager::instance();

  auto window = window_manager.create(1440, 900, "Charlie3D", {.resizable = true});

  auto input_handler = charlie::InputHandler{};
  auto renderer = charlie::Renderer{window};
  input_handler.register_listener(renderer);

  charlie::ArcballCameraController arcball_controller{window};
  charlie::Camera camera{arcball_controller};
  {
    const auto [width, height] = window.resolution();
    camera.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  }
  input_handler.register_listener(camera);

  init_lost_empire_scene(renderer);

  using namespace std::literals::chrono_literals;
  auto previous_time = std::chrono::steady_clock::now();
  std::chrono::steady_clock::duration lag = 0ns;
  static constexpr auto MS_PER_UPDATE = 10ms;
  while (true) {
    const auto current_time = std::chrono::steady_clock::now();
    const auto delta_time = current_time - previous_time;
    previous_time = current_time;
    lag += delta_time;

    input_handler.handle_events();

    draw_gui(window, camera);

    while (lag >= MS_PER_UPDATE) {
      camera.fixed_update();
      lag -= MS_PER_UPDATE;
    }

    renderer.render(camera);

    FrameMark;
  }
}