#ifndef CHARLIE3D_CAMERA_HPP
#define CHARLIE3D_CAMERA_HPP

#include <beyond/math/angle.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/math/point.hpp>

#include "../window/input_handler.hpp"

namespace charlie {

struct CameraController;

class Camera : public InputListener {
public:
  beyond::Radian fovy = beyond::Degree{70};
  float aspect_ratio = 1.0f;
  float z_near = 0.1f;
  float z_far = 200.f;

private:
  CameraController* controller_ = nullptr;

public:
  explicit Camera(CameraController& controller) : controller_{&controller} {}

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4;
  [[nodiscard]] auto proj_matrix() const -> beyond::Mat4;

  void draw_gui();
  void on_input_event(const Event& event, const InputStates& states) override;

  void fixed_update();
};

struct CameraController {
  CameraController() = default;
  virtual ~CameraController() = default;
  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;

  virtual void draw_gui() {}
  virtual void update() {}
  virtual void fixed_update() {}
  [[nodiscard]] virtual auto view_matrix() const -> beyond::Mat4 = 0;

  virtual void on_input_event(const Event&, const InputStates&) {}
  virtual void reset() {}
};

class FirstPersonCameraController : public CameraController {
  beyond::Point3 position_{0.f, 0.f, 0.f};
  beyond::Vec3 input_axis_{0.f, 0.f, 0.f};

  void fixed_update() override;
  [[nodiscard]] auto view_matrix() const -> beyond::Mat4 override;
  void on_key_input(int key, int scancode, int action, int mods);
};

class ArcballCameraController : public CameraController {
public:
  explicit ArcballCameraController(Window& window,
                                   beyond::Point3 initial_eye = beyond::Point3{0, 0, -1},
                                   beyond::Point3 initial_lookat = beyond::Point3{0, 0, 0})
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
  const beyond::Point3 initial_lookat_;
  beyond::Point3 desired_lookat_;
  beyond::Point3 lookat_;

  // The direction from camera to the lookat point
  const beyond::Vec3 initial_forward_axis_;
  beyond::Vec3 forward_axis_;

  static constexpr beyond::Vec3 up_ = beyond::Vec3{0, 1, 0};

  beyond::IPoint2 old_mouse_pos_;

  static constexpr float initial_pan_speed = 1.0f;
  static constexpr float initial_zoom_speed = 0.1f;

  float pan_speed_ = initial_pan_speed;
  float zoom_speed_ = initial_zoom_speed;

  const float initial_zooming_;
  float desired_zooming_;
  float zooming_;

  void fixed_update() override;

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4 override;

  void on_input_event(const Event& e, const InputStates& states) override;
  void on_mouse_move(MouseMoveEvent event, const InputStates& states);
  void on_mouse_scroll(MouseWheelEvent event);

  void draw_gui() override;

  [[nodiscard]] auto right_axis() const -> beyond::Vec3;

  [[nodiscard]] auto eye_position_from_zooming(float zooming) const -> beyond::Point3;

  void reset() override;
};

} // namespace charlie

#endif // CHARLIE3D_CAMERA_HPP
