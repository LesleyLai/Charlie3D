#ifndef CHARLIE3D_GUI_HPP
#define CHARLIE3D_GUI_HPP

#include <beyond/utils/ref.hpp>

#include "utils/framerate_counter.hpp"
#include "window/resolution.hpp"
#include <chrono>

namespace charlie {

class Renderer;
class Camera;

} // namespace charlie

class GUI {
  charlie::Renderer& renderer_;
  charlie::Camera& camera_;
  bool hide_windows_ = false;
  charlie::FramerateCounter framerate_counter_;

public:
  GUI(beyond::Ref<charlie::Renderer> renderer, beyond::Ref<charlie::Camera> camera)
      : renderer_{renderer.get()}, camera_{camera.get()}
  {
  }

  void draw(std::chrono::steady_clock::duration delta_time);

  void toggle_hide_windows() { hide_windows_ = not hide_windows_; }
};

#endif // CHARLIE3D_GUI_HPP
