#ifndef CHARLIE3D_INPUT_HANDLER_HPP
#define CHARLIE3D_INPUT_HANDLER_HPP

#include <variant>
#include <vector>

#include "window.hpp"

namespace charlie {

enum class WindowEventType { resize };

struct WindowEvent {
  u32 window_id{};
  WindowEventType type;
};

enum class MouseButton {
  left = 1,
  middle = 2,
  right = 3,
};
constexpr auto mouse_button_count = 4;

struct MouseButtonEvent {
  enum Type { up = 0, down = 1 };

  Type type;
  MouseButton button;
};

struct MouseMoveEvent {
  int x = 0;
  int y = 0;
};

struct MouseWheelEvent {
  float x = 0;
  float y = 0;
};

using Event = std::variant<WindowEvent, MouseButtonEvent, MouseMoveEvent, MouseWheelEvent>;

class InputStates;

struct InputListener {
  InputListener() = default;
  virtual ~InputListener() = default;

  virtual void on_input_event(const Event& event, const InputStates& states) = 0;
};

class InputStates {
  bool mouse_button_down_[mouse_button_count] = {};

public:
  void set_mouse_button_down(MouseButton button, bool down)
  {
    mouse_button_down_[static_cast<std::underlying_type_t<MouseButton>>(button)] = down;
  }

  [[nodiscard]] auto mouse_button_down(MouseButton button) const
  {
    return mouse_button_down_[static_cast<std::underlying_type_t<MouseButton>>(button)];
  }
};

class InputHandler {
  InputStates states_;
  std::vector<InputListener*> listeners_;

public:
  void handle_events();
  void register_listener(InputListener& listener);
};

} // namespace charlie

#endif // CHARLIE3D_INPUT_HANDLER_HPP
