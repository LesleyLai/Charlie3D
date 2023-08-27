#include "input_handler.hpp"

#include <SDL2/SDL.h>

#include <imgui_impl_sdl2.h>

#include <beyond/types/optional.hpp>

namespace {

[[nodiscard]] auto to_charlie_mouse_button(const uint8_t sdl_button)
    -> beyond::optional<charlie::MouseButton>
{
  using charlie::MouseButton;

  switch (sdl_button) {
  case SDL_BUTTON_LEFT:
    return MouseButton::left;
  case SDL_BUTTON_RIGHT:
    return MouseButton::right;
  case SDL_BUTTON_MIDDLE:
    return MouseButton::middle;
  default:
    return beyond::nullopt;
  }
}

[[nodiscard]] auto to_charlie_event(const SDL_Event& event) -> beyond::optional<charlie::Event>
{
  switch (event.type) {
  case SDL_WINDOWEVENT: {
    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
      return charlie::WindowEvent{.window_id = event.window.windowID,
                                  .type = charlie::WindowEventType::resize};
    }
    return beyond::nullopt;
  }
  case SDL_MOUSEBUTTONUP:
    [[fallthrough]];
  case SDL_MOUSEBUTTONDOWN: {
    using MouseButtonEventType = charlie::MouseButtonEvent::Type;
    const auto button_event_type =
        event.type == SDL_MOUSEBUTTONUP ? MouseButtonEventType::up : MouseButtonEventType::down;

    return to_charlie_mouse_button(event.button.button).map([&](charlie::MouseButton button) {
      return charlie::MouseButtonEvent{
          .type = button_event_type,
          .button = button,
      };
    });
  }
  case SDL_MOUSEMOTION:
    return charlie::MouseMoveEvent{
        .x = event.motion.x,
        .y = event.motion.y,
    };
  case SDL_MOUSEWHEEL:
    return charlie::MouseWheelEvent{.x = event.wheel.preciseX, .y = event.wheel.preciseY};
  default:
    return beyond::nullopt;
  }
}

} // namespace

namespace charlie {

void InputHandler::handle_events()
{
  SDL_Event sdl_event;
  while (SDL_PollEvent(&sdl_event)) {
    ImGui_ImplSDL2_ProcessEvent(&sdl_event);
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse &&
        (sdl_event.type == SDL_MOUSEMOTION || sdl_event.type == SDL_MOUSEBUTTONDOWN ||
         sdl_event.type == SDL_MOUSEBUTTONUP || sdl_event.type == SDL_MOUSEWHEEL)) {
      continue;
    }
    if (io.WantCaptureKeyboard && (sdl_event.type == SDL_KEYUP || sdl_event.type == SDL_KEYDOWN)) {
      continue;
    }

    switch (sdl_event.type) {
    case SDL_QUIT:
      std::exit(0);
    case SDL_MOUSEBUTTONDOWN: {
      to_charlie_mouse_button(sdl_event.button.button).map([&](MouseButton button) {
        states_.set_mouse_button_down(button, true);
      });
    } break;
    case SDL_MOUSEBUTTONUP:
      to_charlie_mouse_button(sdl_event.button.button).map([&](MouseButton button) {
        states_.set_mouse_button_down(button, false);
      });
      break;
    }

    to_charlie_event(sdl_event).map([this](const Event& event) {
      for (auto& listener : listeners_) { listener->on_input_event(event, states_); }
    });
  }
}

void InputHandler::register_listener(InputListener& listener)
{
  listeners_.push_back(&listener);
}

} // namespace charlie