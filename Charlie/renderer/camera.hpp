#ifndef CHARLIE3D_CAMERA_HPP
#define CHARLIE3D_CAMERA_HPP

#include <beyond/math/angle.hpp>
#include <beyond/math/point.hpp>

#include <chrono>

#include "../utils/prelude.hpp"
#include "../window/input_handler.hpp"

namespace charlie {

struct CameraController;

class Camera {
public:
  beyond::Radian fovy = beyond::Degree{70};
  f32 aspect_ratio = 1.0f;
  f32 z_near = 0.1f;
  f32 z_far = 200.f;

private:
  CameraController* controller_ = nullptr;
  std::chrono::steady_clock::duration update_lag_{};

public:
  explicit Camera(CameraController& controller) : controller_{&controller} {}

  [[nodiscard]] auto view_matrix() const -> Mat4;
  [[nodiscard]] auto proj_matrix() const -> Mat4;
  [[nodiscard]] auto position() const -> Vec3;

  void draw_gui();
  void on_input_event(const Event& event, const InputStates& states);

  void update(std::chrono::steady_clock::duration delta_time);
};

struct CameraController {
  CameraController() = default;
  virtual ~CameraController() = default;
  CameraController(const CameraController&) = delete;
  auto operator=(const CameraController&) -> CameraController& = delete;

  virtual void draw_gui() {}
  virtual void update() {}
  virtual void fixed_update() {}
  [[nodiscard]] virtual auto position() const -> Vec3 = 0;
  [[nodiscard]] virtual auto view_matrix() const -> Mat4 = 0;

  virtual void on_input_event(const Event&, const InputStates&) {}
  virtual void reset() {}
};

class FirstPersonCameraController : public CameraController {
  Point3 position_{0.f, 0.f, 0.f};
  Vec3 input_axis_{0.f, 0.f, 0.f};

  void fixed_update() override;
  [[nodiscard]] auto view_matrix() const -> Mat4 override;
  [[nodiscard]] auto position() const -> Vec3 override { return position_; }
  void on_key_input(int key, int scancode, int action, int mods);
};

class ArcballCameraController : public CameraController {
public:
  explicit ArcballCameraController(Window& window, Point3 initial_eye = Point3{0, 0, -1},
                                   Point3 initial_lookat = Point3{0, 0, 0})
      : window_{&window},

        initial_lookat_{initial_lookat}, desired_lookat_{initial_lookat}, lookat_{initial_lookat_},

        initial_forward_axis_{normalize(initial_lookat - initial_eye)},
        forward_axis_{initial_forward_axis_},

        initial_zooming_{(initial_lookat - initial_eye).length()},
        desired_zooming_{initial_zooming_}, zooming_{initial_zooming_}
  {
  }

private:
  Window* window_ = nullptr;

  bool smooth_movement_ = true;

  // the point look at
  const Point3 initial_lookat_;
  Point3 desired_lookat_;
  Point3 lookat_;

  // The direction from camera to the lookat point
  const Vec3 initial_forward_axis_;
  Vec3 forward_axis_;

  static constexpr Vec3 up_ = beyond::Vec3{0, 1, 0};

  // IPoint2
  IPoint2 old_mouse_pos_;

  static constexpr f32 initial_pan_speed = 1.0f;
  static constexpr f32 initial_zoom_speed = 0.1f;

  f32 pan_speed_ = initial_pan_speed;
  f32 zoom_speed_ = initial_zoom_speed;

  const f32 initial_zooming_;
  f32 desired_zooming_;
  f32 zooming_;

  void fixed_update() override;

  [[nodiscard]] auto view_matrix() const -> Mat4 override;

  void on_input_event(const Event& e, const InputStates& states) override;
  void on_mouse_move(MouseMoveEvent event, const InputStates& states);
  void on_mouse_scroll(MouseWheelEvent event);

  void draw_gui() override;

  [[nodiscard]] auto position() const -> Vec3 override
  {
    return eye_position_from_zooming(zooming_);
  }

  [[nodiscard]] auto right_axis() const -> Vec3;

  [[nodiscard]] auto eye_position_from_zooming(f32 zooming) const -> Point3;

  void reset() override;
};

} // namespace charlie

#endif // CHARLIE3D_CAMERA_HPP
