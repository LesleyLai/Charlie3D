#include "window/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "renderer/scene.hpp"

#include "utils/configuration.hpp"
#include "utils/file.hpp"
#include "utils/framerate_counter.hpp"

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <imgui_internal.h>

#include <SDL2/SDL.h>

#include <tracy/Tracy.hpp>

#include <chrono>

void set_asset_path()
{
  const auto current_path = std::filesystem::current_path();
  auto asset_path = charlie::locate_asset_path().expect("Cannot find assets folder!");

  Configurations::instance().set(CONFIG_ASSETS_PATH, asset_path);
}

void draw_gui(charlie::Resolution resolution, charlie::Renderer& renderer, charlie::Camera& camera,
              std::chrono::steady_clock::duration delta_time)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow();

  bool show_control_panel = true;
  const float control_panel_width = ImGui::GetFontSize() * 25.f;
  ImGui::SetNextWindowPos(ImVec2(beyond::narrow<float>(resolution.width) - control_panel_width, 0));
  ImGui::SetNextWindowSize(ImVec2(control_panel_width, beyond::narrow<float>(resolution.height)));
  ImGui::Begin("Control Panel", &show_control_panel, ImGuiWindowFlags_NoDecoration);

  if (ImGui::CollapsingHeader("Environment Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& scene_parameters = renderer.scene_parameters();

    ImGui::SliderFloat("Ambient Strength", &scene_parameters.sunlight_direction.w, 0.1f, 10, "%.3f",
                       ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

    ImGui::Text("Sunlight");
    ImGui::LabelText("Direction", "%f %f %f", scene_parameters.sunlight_direction.x,
                     scene_parameters.sunlight_direction.y, scene_parameters.sunlight_direction.z);

    int rgb[3] = {static_cast<uint8_t>(scene_parameters.sunlight_color.x * 255),
                  static_cast<uint8_t>(scene_parameters.sunlight_color.y * 255),
                  static_cast<uint8_t>(scene_parameters.sunlight_color.z * 255)};
    ImGui::SliderInt3("color", rgb, 0, 255);
    scene_parameters.sunlight_color.x = static_cast<float>(rgb[0]) / 255.f;
    scene_parameters.sunlight_color.y = static_cast<float>(rgb[1]) / 255.f;
    scene_parameters.sunlight_color.z = static_cast<float>(rgb[2]) / 255.f;
    ImGui::SliderFloat("intensity", &scene_parameters.sunlight_color.w, 0, 10000, "%.3f",
                       ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
  }

  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) { camera.draw_gui(); }

  static charlie::FramerateCounter framerate_counter;
  framerate_counter.update(delta_time);
  if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
    const auto& scene = renderer.scene();

    ImGui::LabelText("Viewport", "%ux%u", resolution.width, resolution.height);

    ImGui::SeparatorText("Scene Data");
    ImGui::LabelText("Nodes", "%zu", scene.local_transforms.size());

    ImGui::SeparatorText("Performance Data");
    ImGui::LabelText("FPS", "%.0f", 1e3f / framerate_counter.average_ms_per_frame);
    ImGui::LabelText("ms per frame", "%.2f", framerate_counter.average_ms_per_frame);
  }

  ImGui::End();
}

int main(int argc, const char** argv)
{
  std::string_view scene_file = "models/gltf_box/box.gltf";
  if (argc == 2) { scene_file = argv[1]; }

  set_asset_path();

  auto window =
      charlie::WindowManager::instance().create(1440, 900, "Charlie3D", {.resizable = true});

  auto input_handler = charlie::InputHandler{};

  auto renderer = [&]() {
    ZoneScopedN("Renderer Constructor");
    return charlie::Renderer{window};
  }();
  input_handler.register_listener(renderer);

  charlie::ArcballCameraController arcball_controller{window, beyond::Point3{0, 0, -2},
                                                      beyond::Point3{0, 0, 0}};
  charlie::Camera camera{arcball_controller};
  {
    const auto [width, height] = window.resolution();
    camera.aspect_ratio = beyond::narrow<float>(width) / beyond::narrow<float>(height);
  }
  input_handler.register_listener(camera);

  renderer.set_scene(std::make_unique<charlie::Scene>(charlie::load_scene(scene_file, renderer)));

  using Clock = std::chrono::steady_clock;
  using namespace std::literals::chrono_literals;
  auto previous_time = Clock::now();
  Clock::duration lag = 0ns;
  static constexpr auto MS_PER_FIXED_UPDATE = 10ms;
  while (true) {
    const auto current_time = Clock::now();
    const auto delta_time = current_time - previous_time;
    previous_time = current_time;
    lag += delta_time;

    input_handler.handle_events();

    draw_gui(window.resolution(), renderer, camera, delta_time);

    while (lag >= MS_PER_FIXED_UPDATE) {
      camera.fixed_update();
      lag -= MS_PER_FIXED_UPDATE;
    }

    renderer.render(camera);

    FrameMark;
  }
}