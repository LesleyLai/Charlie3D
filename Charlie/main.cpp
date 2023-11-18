#include "window/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "renderer/scene.hpp"

#include "utils/configuration.hpp"
#include "utils/file.hpp"
#include "utils/file_watcher.hpp"
#include "utils/framerate_counter.hpp"

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <chrono>

void set_asset_path()
{
  const auto current_path = std::filesystem::current_path();
  auto asset_path = charlie::locate_asset_path().expect("Cannot find assets folder!");

  Configurations::instance().set(CONFIG_ASSETS_PATH, asset_path);
}

static bool hide_control_panel = false;

void draw_gui(charlie::Resolution resolution, charlie::Renderer& renderer, charlie::Camera& camera,
              std::chrono::steady_clock::duration delta_time)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // ImGui::ShowDemoWindow();

  if (hide_control_panel) { return; }

  const float control_panel_width = ImGui::GetFontSize() * 30.f;
  ImGui::SetNextWindowPos(ImVec2(beyond::narrow<float>(resolution.width) - control_panel_width, 0));
  ImGui::SetNextWindowSize(ImVec2(control_panel_width, beyond::narrow<float>(resolution.height)));
  ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_NoDecoration);

  if (ImGui::CollapsingHeader("Environment Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& scene_parameters = renderer.scene_parameters();

    ImGui::SeparatorText("Ambient");
    ImGui::SliderFloat("Intensity", &scene_parameters.sunlight_direction.w, 0, 10, "%.3f",
                       ImGuiSliderFlags_AlwaysClamp);

    ImGui::SeparatorText("Sunlight");
    ImGui::PushID("Sunlight");
    static float phi_degree = 0;

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

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize()));
  }

  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    camera.draw_gui();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize()));
  }

  static charlie::FramerateCounter framerate_counter;
  framerate_counter.update(delta_time);
  if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
    const auto& scene = renderer.scene();

    ImGui::LabelText("Viewport", "%ux%u", resolution.width, resolution.height);

    ImGui::SeparatorText("Scene Data");
    ImGui::LabelText("Nodes", "%zu", scene.local_transforms.size());

    ImGui::SeparatorText("Performance Data");
    ImGui::LabelText("FPS", "%.0f", 1e3f / framerate_counter.average_ms_per_frame);
    ImGui::LabelText("ms/frame", "%.2f", framerate_counter.average_ms_per_frame);

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize()));
  }

  ImGui::End();
}

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

int main(int argc, const char** argv)
{
  std::string_view scene_file = "models/gltf_box/box.gltf";
  if (argc == 2) { scene_file = argv[1]; }

  set_asset_path();

  const auto asset_path = Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);

  auto window = charlie::WindowManager::instance().create(1440, 900, "Charlie3D",
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