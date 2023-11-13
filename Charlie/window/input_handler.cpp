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

[[nodiscard]] auto to_charlie_keycode(const SDL_Keycode keycode) -> charlie::KeyCode
{
  using enum charlie::KeyCode;

  switch (static_cast<SDL_KeyCode>(keycode)) {
  case SDLK_UNKNOWN:
    return unknown;
  case SDLK_RETURN:
    return return_key;
  case SDLK_ESCAPE:
    return escape;
  case SDLK_BACKSPACE:
    return backspace;
  case SDLK_TAB:
    return tab;
  case SDLK_SPACE:
    return space;
  case SDLK_EXCLAIM:
    return exclaim;
  case SDLK_QUOTEDBL:
    return quotedbl;
  case SDLK_HASH:
    return hash;
  case SDLK_PERCENT:
    return percent;
  case SDLK_DOLLAR:
    return dollar;
  case SDLK_AMPERSAND:
    return ampersand;
  case SDLK_QUOTE:
    return quote;
  case SDLK_LEFTPAREN:
    return left_paren;
  case SDLK_RIGHTPAREN:
    return right_paren;
  case SDLK_ASTERISK:
    return asterisk;
  case SDLK_PLUS:
    return plus;
  case SDLK_COMMA:
    return comma;
  case SDLK_MINUS:
    return minus;
  case SDLK_PERIOD:
    return period;
  case SDLK_SLASH:
    return slash;
  case SDLK_0:
    return _0;
  case SDLK_1:
    return _1;
  case SDLK_2:
    return _2;
  case SDLK_3:
    return _3;
  case SDLK_4:
    return _4;
  case SDLK_5:
    return _5;
  case SDLK_6:
    return _6;
  case SDLK_7:
    return _7;
  case SDLK_8:
    return _8;
  case SDLK_9:
    return _9;
  case SDLK_COLON:
    return colon;
  case SDLK_SEMICOLON:
    return semicolon;
  case SDLK_LESS:
    return less;
  case SDLK_EQUALS:
    return equals;
  case SDLK_GREATER:
    return greater;
  case SDLK_QUESTION:
    return question;
  case SDLK_AT:
    return at;
  case SDLK_LEFTBRACKET:
    return left_bracket;
  case SDLK_BACKSLASH:
    return backslash;
  case SDLK_RIGHTBRACKET:
    return right_bracket;
  case SDLK_CARET:
    return caret;
  case SDLK_UNDERSCORE:
    return underscore;
  case SDLK_BACKQUOTE:
    return backquote;
  case SDLK_a:
    return a;
  case SDLK_b:
    return b;
  case SDLK_c:
    return c;
  case SDLK_d:
    return d;
  case SDLK_e:
    return e;
  case SDLK_f:
    return f;
  case SDLK_g:
    return g;
  case SDLK_h:
    return h;
  case SDLK_i:
    return i;
  case SDLK_j:
    return j;
  case SDLK_k:
    return k;
  case SDLK_l:
    return l;
  case SDLK_m:
    return m;
  case SDLK_n:
    return n;
  case SDLK_o:
    return o;
  case SDLK_p:
    return p;
  case SDLK_q:
    return q;
  case SDLK_r:
    return r;
  case SDLK_s:
    return s;
  case SDLK_t:
    return t;
  case SDLK_u:
    return u;
  case SDLK_v:
    return v;
  case SDLK_w:
    return w;
  case SDLK_x:
    return x;
  case SDLK_y:
    return y;
  case SDLK_z:
    return z;
  case SDLK_CAPSLOCK:
    return capslock;
  case SDLK_F1:
    return f1;
  case SDLK_F2:
    return f2;
  case SDLK_F3:
    return f3;
  case SDLK_F4:
    return f4;
  case SDLK_F5:
    return f5;
  case SDLK_F6:
    return f6;
  case SDLK_F7:
    return f7;
  case SDLK_F8:
    return f8;
  case SDLK_F9:
    return f9;
  case SDLK_F10:
    return f10;
  case SDLK_F11:
    return f11;
  case SDLK_F12:
    return f12;
  case SDLK_PRINTSCREEN:
    break;
  case SDLK_SCROLLLOCK:
    break;
  case SDLK_PAUSE:
    break;
  case SDLK_INSERT:
    break;
  case SDLK_HOME:
    break;
  case SDLK_PAGEUP:
    break;
  case SDLK_DELETE:
    break;
  case SDLK_END:
    break;
  case SDLK_PAGEDOWN:
    break;
  case SDLK_RIGHT:
    break;
  case SDLK_LEFT:
    break;
  case SDLK_DOWN:
    break;
  case SDLK_UP:
    break;
  case SDLK_NUMLOCKCLEAR:
    break;
  case SDLK_KP_DIVIDE:
    break;
  case SDLK_KP_MULTIPLY:
    break;
  case SDLK_KP_MINUS:
    break;
  case SDLK_KP_PLUS:
    break;
  case SDLK_KP_ENTER:
    break;
  case SDLK_KP_1:
    break;
  case SDLK_KP_2:
    break;
  case SDLK_KP_3:
    break;
  case SDLK_KP_4:
    break;
  case SDLK_KP_5:
    break;
  case SDLK_KP_6:
    break;
  case SDLK_KP_7:
    break;
  case SDLK_KP_8:
    break;
  case SDLK_KP_9:
    break;
  case SDLK_KP_0:
    break;
  case SDLK_KP_PERIOD:
    break;
  case SDLK_APPLICATION:
    break;
  case SDLK_POWER:
    break;
  case SDLK_KP_EQUALS:
    break;
  case SDLK_F13:
    break;
  case SDLK_F14:
    break;
  case SDLK_F15:
    break;
  case SDLK_F16:
    break;
  case SDLK_F17:
    break;
  case SDLK_F18:
    break;
  case SDLK_F19:
    break;
  case SDLK_F20:
    break;
  case SDLK_F21:
    break;
  case SDLK_F22:
    break;
  case SDLK_F23:
    break;
  case SDLK_F24:
    break;
  case SDLK_EXECUTE:
    break;
  case SDLK_HELP:
    break;
  case SDLK_MENU:
    break;
  case SDLK_SELECT:
    break;
  case SDLK_STOP:
    break;
  case SDLK_AGAIN:
    break;
  case SDLK_UNDO:
    break;
  case SDLK_CUT:
    break;
  case SDLK_COPY:
    break;
  case SDLK_PASTE:
    break;
  case SDLK_FIND:
    break;
  case SDLK_MUTE:
    break;
  case SDLK_VOLUMEUP:
    break;
  case SDLK_VOLUMEDOWN:
    break;
  case SDLK_KP_COMMA:
    break;
  case SDLK_KP_EQUALSAS400:
    break;
  case SDLK_ALTERASE:
    break;
  case SDLK_SYSREQ:
    break;
  case SDLK_CANCEL:
    break;
  case SDLK_CLEAR:
    break;
  case SDLK_PRIOR:
    break;
  case SDLK_RETURN2:
    break;
  case SDLK_SEPARATOR:
    break;
  case SDLK_OUT:
    break;
  case SDLK_OPER:
    break;
  case SDLK_CLEARAGAIN:
    break;
  case SDLK_CRSEL:
    break;
  case SDLK_EXSEL:
    break;
  case SDLK_KP_00:
    break;
  case SDLK_KP_000:
    break;
  case SDLK_THOUSANDSSEPARATOR:
    break;
  case SDLK_DECIMALSEPARATOR:
    break;
  case SDLK_CURRENCYUNIT:
    break;
  case SDLK_CURRENCYSUBUNIT:
    break;
  case SDLK_KP_LEFTPAREN:
    break;
  case SDLK_KP_RIGHTPAREN:
    break;
  case SDLK_KP_LEFTBRACE:
    break;
  case SDLK_KP_RIGHTBRACE:
    break;
  case SDLK_KP_TAB:
    break;
  case SDLK_KP_BACKSPACE:
    break;
  case SDLK_KP_A:
    break;
  case SDLK_KP_B:
    break;
  case SDLK_KP_C:
    break;
  case SDLK_KP_D:
    break;
  case SDLK_KP_E:
    break;
  case SDLK_KP_F:
    break;
  case SDLK_KP_XOR:
    break;
  case SDLK_KP_POWER:
    break;
  case SDLK_KP_PERCENT:
    break;
  case SDLK_KP_LESS:
    break;
  case SDLK_KP_GREATER:
    break;
  case SDLK_KP_AMPERSAND:
    break;
  case SDLK_KP_DBLAMPERSAND:
    break;
  case SDLK_KP_VERTICALBAR:
    break;
  case SDLK_KP_DBLVERTICALBAR:
    break;
  case SDLK_KP_COLON:
    break;
  case SDLK_KP_HASH:
    break;
  case SDLK_KP_SPACE:
    break;
  case SDLK_KP_AT:
    break;
  case SDLK_KP_EXCLAM:
    break;
  case SDLK_KP_MEMSTORE:
    break;
  case SDLK_KP_MEMRECALL:
    break;
  case SDLK_KP_MEMCLEAR:
    break;
  case SDLK_KP_MEMADD:
    break;
  case SDLK_KP_MEMSUBTRACT:
    break;
  case SDLK_KP_MEMMULTIPLY:
    break;
  case SDLK_KP_MEMDIVIDE:
    break;
  case SDLK_KP_PLUSMINUS:
    break;
  case SDLK_KP_CLEAR:
    break;
  case SDLK_KP_CLEARENTRY:
    break;
  case SDLK_KP_BINARY:
    break;
  case SDLK_KP_OCTAL:
    break;
  case SDLK_KP_DECIMAL:
    break;
  case SDLK_KP_HEXADECIMAL:
    break;
  case SDLK_LCTRL:
    break;
  case SDLK_LSHIFT:
    break;
  case SDLK_LALT:
    break;
  case SDLK_LGUI:
    break;
  case SDLK_RCTRL:
    break;
  case SDLK_RSHIFT:
    break;
  case SDLK_RALT:
    break;
  case SDLK_RGUI:
    break;
  case SDLK_MODE:
    break;
  case SDLK_AUDIONEXT:
    break;
  case SDLK_AUDIOPREV:
    break;
  case SDLK_AUDIOSTOP:
    break;
  case SDLK_AUDIOPLAY:
    break;
  case SDLK_AUDIOMUTE:
    break;
  case SDLK_MEDIASELECT:
    break;
  case SDLK_WWW:
    break;
  case SDLK_MAIL:
    break;
  case SDLK_CALCULATOR:
    break;
  case SDLK_COMPUTER:
    break;
  case SDLK_AC_SEARCH:
    break;
  case SDLK_AC_HOME:
    break;
  case SDLK_AC_BACK:
    break;
  case SDLK_AC_FORWARD:
    break;
  case SDLK_AC_STOP:
    break;
  case SDLK_AC_REFRESH:
    break;
  case SDLK_AC_BOOKMARKS:
    break;
  case SDLK_BRIGHTNESSDOWN:
    break;
  case SDLK_BRIGHTNESSUP:
    break;
  case SDLK_DISPLAYSWITCH:
    break;
  case SDLK_KBDILLUMTOGGLE:
    break;
  case SDLK_KBDILLUMDOWN:
    break;
  case SDLK_KBDILLUMUP:
    break;
  case SDLK_EJECT:
    break;
  case SDLK_SLEEP:
    break;
  case SDLK_APP1:
    break;
  case SDLK_APP2:
    break;
  case SDLK_AUDIOREWIND:
    break;
  case SDLK_AUDIOFASTFORWARD:
    break;
  case SDLK_SOFTLEFT:
    break;
  case SDLK_SOFTRIGHT:
    break;
  case SDLK_CALL:
    break;
  case SDLK_ENDCALL:
    break;
  }

  return unknown;
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
  case SDL_KEYUP:
    [[fallthrough]];
  case SDL_KEYDOWN: {
    using KeyBoardEventType = charlie::KeyboardEvent::Type;

    const auto keyboard_event_type =
        event.type == SDL_KEYDOWN ? KeyBoardEventType::down : KeyBoardEventType::up;

    const SDL_KeyboardEvent key_event = event.key;
    const auto state = key_event.state == SDL_PRESSED ? charlie::PressReleaseState::pressed
                                                      : charlie::PressReleaseState::released;

    const SDL_Keysym keysym = key_event.keysym;

    return charlie::KeyboardEvent{
        .type = keyboard_event_type, .state = state, .keycode = to_charlie_keycode(keysym.sym)};
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
      for (auto& listener : listeners_) { listener(event, states_); }
    });
  }
}

} // namespace charlie