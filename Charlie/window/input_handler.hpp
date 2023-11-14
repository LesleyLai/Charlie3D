#ifndef CHARLIE3D_INPUT_HANDLER_HPP
#define CHARLIE3D_INPUT_HANDLER_HPP

#include <beyond/container/slot_map.hpp>
#include <beyond/utils/unique_function.hpp>
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

enum class PressReleaseState { pressed, released };

enum class KeyCode {
  unknown,
  return_key = '\r',
  escape = '\x1B',
  backspace = '\b',
  tab = '\t',
  space = ' ',
  exclaim = '!',
  quotedbl = '"',
  hash = '#',
  percent = '%',
  dollar = '$',
  ampersand = '&',
  quote = '\'',
  left_paren = '(',
  right_paren = ')',
  asterisk = '*',
  plus = '+',
  comma = ',',
  minus = '-',
  period = '.',
  slash = '/',
  _0 = '0',
  _1 = '1',
  _2 = '2',
  _3 = '3',
  _4 = '4',
  _5 = '5',
  _6 = '6',
  _7 = '7',
  _8 = '8',
  _9 = '9',
  colon = ':',
  semicolon = ';',
  less = '<',
  equals = '=',
  greater = '>',
  question = '?',
  at = '@',
  left_bracket = '[',
  backslash = '\\',
  right_bracket = ']',
  caret = '^',
  underscore = '_',
  backquote = '`',
  a = 'a',
  b = 'b',
  c = 'c',
  d = 'd',
  e = 'e',
  f = 'f',
  g = 'g',
  h = 'h',
  i = 'i',
  j = 'j',
  k = 'k',
  l = 'l',
  m = 'm',
  n = 'n',
  o = 'o',
  p = 'p',
  q = 'q',
  r = 'r',
  s = 's',
  t = 't',
  u = 'u',
  v = 'v',
  w = 'w',
  x = 'x',
  y = 'y',
  z = 'z',

  capslock,

  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  f9,
  f10,
  f11,
  f12,

  kp_0,
  kp_1,
  kp_2,
  kp_3,
  kp_4,
  kp_5,
  kp_6,
  kp_7,
  kp_8,
  kp_9,
};

struct KeyboardEvent {
  enum class Type { down, up };

  Type type;
  PressReleaseState state;
  KeyCode keycode;
};

using Event =
    std::variant<WindowEvent, MouseButtonEvent, MouseMoveEvent, MouseWheelEvent, KeyboardEvent>;

class InputStates;

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

using InputListener = beyond::unique_function<void(const Event&, const InputStates&)>;

// A handle for InputListener callback. Used to remove an InputListener from the InputHandler
struct InputListenerHandle : beyond::GenerationalHandle<InputListenerHandle, u32, 16> {
  using GenerationalHandle::GenerationalHandle;
};

class InputHandler {
  InputStates states_;
  beyond::SlotMap<InputListenerHandle, InputListener> listeners_;

public:
  void handle_events();

  auto add_listener(InputListener listener) -> InputListenerHandle
  {
    return listeners_.insert(BEYOND_MOV(listener));
  }

  void remove_listener(InputListenerHandle handle) { listeners_.erase(handle); }

  // Helper functions to add listeners that only care about specific types of input events
  auto add_keyboard_event_listener(
      beyond::unique_function<void(const KeyboardEvent&, const InputStates&)> listener)
      -> InputListenerHandle;
};

// An RAII guard for InputListener
class [[nodiscard]] ScopedInputListener {
  InputHandler& handler_;
  InputListenerHandle handle_;

public:
  ScopedInputListener(InputHandler& handler, InputListenerHandle handle)
      : handler_{handler}, handle_{handle}
  {
  }

  ~ScopedInputListener() { handler_.remove_listener(handle_); }

  ScopedInputListener(ScopedInputListener&) = delete;
  ScopedInputListener operator=(ScopedInputListener&) = delete;
};

} // namespace charlie

#endif // CHARLIE3D_INPUT_HANDLER_HPP
