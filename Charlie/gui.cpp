#include "gui.hpp"

#include "renderer/imgui_impl_vulkan.h"
#include <imgui_impl_sdl2.h>

#include <beyond/math/constants.hpp>
#include <beyond/utils/narrowing.hpp>
#include <beyond/utils/zstring_view.hpp>

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"

#include "renderer/scene.hpp"

#include "tinyfiledialogs.h"

using beyond::narrow;
using beyond::u32;

namespace {

void draw_gui_main_menu(charlie::Renderer& renderer)
{
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Model", "Ctrl+O")) {
        const char* filter_patterns[] = {"*.gltf", "*.glb", "*.obj"};
        const auto* result =
            tinyfd_openFileDialog("title", nullptr, narrow<int>(std::size(filter_patterns)),
                                  filter_patterns, "model files", 0);
        if (result != nullptr) {
          renderer.set_scene(
              std::make_unique<charlie::Scene>(charlie::load_scene(result, renderer)));
        }
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void draw_gui_main_window(charlie::Renderer& renderer, const ImGuiViewport& viewport)
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

  draw_gui_main_menu(renderer);

  ImGuiID dockspace_id = ImGui::GetID("Dockspace");
  ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  ImGui::End();
}

void draw_gui_stats_window(charlie::Renderer& renderer,
                           std::chrono::steady_clock::duration delta_time, charlie::Resolution res,
                           charlie::FramerateCounter& framerate_counter)
{
  ImGui::Begin("Stats");
  framerate_counter.update(delta_time);
  const auto& scene = renderer.scene();

  ImGui::LabelText("Viewport", "%ux%u", res.width, res.height);

  ImGui::SeparatorText("Scene Data");
  ImGui::LabelText("Nodes", "%u", scene.node_count());

  ImGui::SeparatorText("Performance Data");
  ImGui::LabelText("FPS", "%.0f", 1e3f / framerate_counter.average_ms_per_frame);
  ImGui::LabelText("ms/frame", "%.2f", framerate_counter.average_ms_per_frame);

  ImGui::End();
}

void draw_gui_shadow_options(beyond::Ref<uint32_t> in_shadow_mode)
{
  int shadow_mode = beyond::narrow<int>(in_shadow_mode.get());

  int enable_shadow_map = shadow_mode > 0;
  ImGui::RadioButton("Disabled", &enable_shadow_map, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Shadow Map", &enable_shadow_map, 1);

  static int shadow_map_mode = 3;

  if (enable_shadow_map == 0) {
    shadow_mode = 0;
  } else {
    if (ImGui::TreeNodeEx("Shadow Mapping Options", ImGuiTreeNodeFlags_DefaultOpen)) {

      ImGui::RadioButton("Hard Shadow", &shadow_map_mode, 1);
      ImGui::SameLine();
      ImGui::RadioButton("PCF", &shadow_map_mode, 2);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Percentage-Closer Filtering is a technique to produce anti-aliased shadow");
      }
      ImGui::SameLine();
      ImGui::RadioButton("PCSS", &shadow_map_mode, 3);
      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Percentage-Closer Soft Shadows"); }

      ImGui::TreePop();
    }
    shadow_mode = shadow_map_mode;
  }

  in_shadow_mode.get() = beyond::narrow<uint32_t>(shadow_mode);
}

} // anonymous namespace

void charlie::Renderer::draw_gui_lighting_window()
{
  ImGui::Begin("Environment Lighting");

  ImGui::SeparatorText("Ambient");
  ImGui::SliderFloat("Intensity", &scene_parameters_.sunlight_direction.w, 0, 10, "%.3f",
                     ImGuiSliderFlags_AlwaysClamp);

  ImGui::SeparatorText("Sunlight");
  ImGui::PushID("Sunlight");
  static float theta = 30.f / 180.f * beyond::float_constants::pi;
  static float phi = 0;

  ImGui::SliderAngle("polar (theta)", &theta, 0, 90);
  ImGui::SliderAngle("azimuthal (phi)", &phi, 0, 360);
  beyond::Vec3 sunlight_direction = {sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)};
  sunlight_direction = -sunlight_direction;

  scene_parameters_.sunlight_direction.xyz = sunlight_direction;

  ImGui::ColorEdit3("Sunlight Color", scene_parameters_.sunlight_color.elem, 0);
  ImGui::SliderFloat("Intensity", &scene_parameters_.sunlight_color.w, 0, 10000, "%.3f",
                     ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

  ImGui::SeparatorText("Sunlight Shadow");

  draw_gui_shadow_options(beyond::ref(scene_parameters_.sunlight_shadow_mode));

  ImGui::PopID();
  ImGui::End();
}

void GUI::draw(const std::chrono::steady_clock::duration delta_time)
{

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  draw_gui_main_window(renderer_, *viewport);

  const charlie::Resolution res{.width = narrow<u32>(viewport->Size.x),
                                .height = narrow<u32>(viewport->Size.y)};
  if (not hide_windows_) {
    renderer_.draw_gui_lighting_window();
    camera_.draw_gui_window();
    draw_gui_stats_window(renderer_, delta_time, res, framerate_counter_);
  }
}
