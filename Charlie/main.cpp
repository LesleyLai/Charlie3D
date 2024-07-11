#include "window/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include "renderer/camera.hpp"
#include "renderer/renderer.hpp"
#include "renderer/scene.hpp"

#include "utils/file.hpp"
#include "utils/file_watcher.hpp"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include "gui.hpp"
#include <imgui.h>

#include <tracy/Tracy.hpp>

#include <beyond/utils/narrowing.hpp>
#include <beyond/utils/zstring_view.hpp>

#include <chrono>
#include <string_view>

using beyond::ref;

auto main(int argc, const char** argv) -> int
{
  std::string_view scene_file = "models/gltf_box/box.gltf";
  if (argc == 2) { scene_file = argv[1]; }

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

  GUI gui{ref(renderer), ref(camera)};

  auto camera_input_listener = charlie::ScopedInputListener(
      input_handler, input_handler.add_listener(
                         std::bind_front(&charlie::Camera::on_input_event, std::ref(camera))));

  input_handler.add_keyboard_event_listener(
      [&gui](const charlie::KeyboardEvent& event, const charlie::InputStates& /*states*/) {
        if (event.state == charlie::PressReleaseState::pressed &&
            event.keycode == charlie::KeyCode::f4) {
          gui.toggle_hide_windows();

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
      gui.draw(delta_time);
      renderer.render(camera);
    }

    FrameMark;
  }
}