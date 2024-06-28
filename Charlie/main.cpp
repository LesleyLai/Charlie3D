#include "window/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "renderer/scene.hpp"

#include "utils/file.hpp"
#include "utils/file_watcher.hpp"
#include "utils/framerate_counter.hpp"

#include "renderer/imgui_impl_vulkan.h"
#include <imgui_impl_sdl2.h>

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <beyond/utils/zstring_view.hpp>
#include <chrono>
#include <string_view>

static bool hide_control_panel = false;

void draw_gui_main_menu()
{
  static constexpr beyond::ZStringView default_ini_path = "config/default_layout.ini";

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Model", "Ctrl+O")) {}
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void draw_gui_main_window(const ImGuiViewport& viewport)
{
  ImGui::SetNextWindowPos(viewport.Pos);
  ImGui::SetNextWindowSize(viewport.Size);
  ImGui::SetNextWindowViewport(viewport.ID);
  ImGui::SetNextWindowBgAlpha(0.0f);

  constexpr ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  draw_gui_main_menu();

  ImGuiID dockspace_id = ImGui::GetID("Dockspace");
  ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  ImGui::End();
}

void draw_gui_stats(charlie::Renderer& renderer, std::chrono::steady_clock::duration delta_time,
                    const ImGuiViewport& viewport)
{
  ImGui::Begin("Stats");
  static charlie::FramerateCounter framerate_counter;
  framerate_counter.update(delta_time);
  const auto& scene = renderer.scene();

  ImGui::LabelText("Viewport", "%ux%u", static_cast<uint32_t>(viewport.Size.x),
                   static_cast<uint32_t>(viewport.Size.y));

  ImGui::SeparatorText("Scene Data");
  ImGui::LabelText("Nodes", "%zu", scene.local_transforms.size());

  ImGui::SeparatorText("Performance Data");
  ImGui::LabelText("FPS", "%.0f", 1e3f / framerate_counter.average_ms_per_frame);
  ImGui::LabelText("ms/frame", "%.2f", framerate_counter.average_ms_per_frame);

  ImGui::End();
}

void draw_gui(charlie::Renderer& renderer, charlie::Camera& camera,
              std::chrono::steady_clock::duration delta_time)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  draw_gui_main_window(*viewport);

  ImGui::ShowDemoWindow();

  if (hide_control_panel) { return; }

  // const float control_panel_width = ImGui::GetFontSize() * 30.f;
  // ImGui::SetNextWindowPos(ImVec2(beyond::narrow<float>(resolution.width) - control_panel_width,
  // 0)); ImGui::SetNextWindowSize(ImVec2(control_panel_width,
  // beyond::narrow<float>(resolution.height)));
  ImGui::Begin("Environment Lighting");

  auto& scene_parameters = renderer.scene_parameters();

  ImGui::SeparatorText("Ambient");
  ImGui::SliderFloat("Intensity", &scene_parameters.sunlight_direction.w, 0, 10, "%.3f",
                     ImGuiSliderFlags_AlwaysClamp);

  ImGui::SeparatorText("Sunlight");
  ImGui::PushID("Sunlight");
  static float theta = 30.f / 180.f * beyond::float_constants::pi;
  static float phi = 0;

  ImGui::SliderAngle("polar (theta)", &theta, 0, 90);
  ImGui::SliderAngle("azimuthal (phi)", &phi, 0, 360);
  beyond::Vec3 sunlight_direction = {sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)};
  sunlight_direction = -sunlight_direction;

  scene_parameters.sunlight_direction.xyz = sunlight_direction;

  ImGui::ColorEdit3("Sunlight Color", scene_parameters.sunlight_color.elem, 0);
  ImGui::SliderFloat("Intensity", &scene_parameters.sunlight_color.w, 0, 10000, "%.3f",
                     ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

  ImGui::SeparatorText("Sunlight Shadow");

  int shadow_mode = renderer.enable_shadow_mapping ? 1 : 0;
  ImGui::RadioButton("Disabled", &shadow_mode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Shadow Map", &shadow_mode, 1);

  renderer.enable_shadow_mapping = shadow_mode == 1;

  //    if (shadow_mode == 1 &&
  //        ImGui::TreeNodeEx("Shadow Mapping Options", ImGuiTreeNodeFlags_DefaultOpen)) {
  //      static bool enable_pcf = true;
  //      ImGui::Checkbox("PCF", &enable_pcf);
  //      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Percentage-Closer Filtering (PCF)"); }
  //
  //      if (enable_pcf) {
  //        ImGui::SeparatorText("PCF Options");
  //        static int pcf_samples = 5;
  //        ImGui::SliderInt("PCF Samples", &pcf_samples, 1, 16);
  //
  //        static float pcf_radius = 0.005f;
  //        ImGui::SliderFloat("PCF Radius", &pcf_radius, 0, 0.01f, "%.4f");
  //      }
  //
  //      ImGui::TreePop();
  //    }

  ImGui::PopID();
  ImGui::End();

  ImGui::Begin("Camera");
  camera.draw_gui();
  ImGui::End();

  draw_gui_stats(renderer, delta_time, *viewport);
}

int main(int argc, const char** argv)
{
  std::string_view scene_file = "models/gltf_box/box.gltf";
  if (argc == 2) { scene_file = argv[1]; }

  auto window = charlie::WindowManager::instance().create(1440, 900, "Charlie3D Model Viewer",
                                                          {.resizable = true, .maximized = true});

  auto input_handler = charlie::InputHandler{};

  auto renderer = [&]() {
    ZoneScopedN("Renderer Constructor");
    return charlie::Renderer{window, input_handler};
  }();

  charlie::ArcballCameraController arcball_controller{window, beyond::Point3{0, 0, -2},
                                                      beyond::Point3{0, 0, 0}};
  charlie::Camera camera{arcball_controller};
  {
    const auto [width, height] = window.resolution();
    camera.aspect_ratio = beyond::narrow<float>(width) / beyond::narrow<float>(height);
  }

  auto camera_input_listener = charlie::ScopedInputListener(
      input_handler, input_handler.add_listener(
                         std::bind_front(&charlie::Camera::on_input_event, std::ref(camera))));

  input_handler.add_keyboard_event_listener(
      [](const charlie::KeyboardEvent& event, const charlie::InputStates& /*states*/) {
        if (event.state == charlie::PressReleaseState::pressed &&
            event.keycode == charlie::KeyCode::f4) {
          hide_control_panel = not hide_control_panel;

          ImGui::SetNextFrameWantCaptureKeyboard(false);
        }
      });

  renderer.set_scene(std::make_unique<charlie::Scene>(charlie::load_scene(scene_file, renderer)));

  using Clock = std::chrono::steady_clock;
  using namespace std::literals::chrono_literals;
  auto previous_time = Clock::now();
  while (true) {
    const auto current_time = Clock::now();
    const auto delta_time = current_time - previous_time;
    previous_time = current_time;

    input_handler.handle_events();

    camera.update(delta_time);

    if (not window.is_minimized()) {
      draw_gui(renderer, camera, delta_time);
      renderer.render(camera);
    }

    FrameMark;
  }
}