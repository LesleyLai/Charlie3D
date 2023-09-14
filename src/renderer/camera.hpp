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
      : window_{&window}, initial_eye_{initial_eye}, initial_lookat_{initial_lookat},
        eye_{initial_eye}, lookat_{initial_lookat_}
  {
  }

private:
  Window* window_ = nullptr;

  const beyond::Point3 initial_eye_;
  const beyond::Point3 initial_lookat_;

  beyond::Point3 eye_;    // camera position
  beyond::Point3 lookat_; // the point look at
  static constexpr beyond::Vec3 up_ = beyond::Vec3{0, 1, 0};

  beyond::IPoint2 old_mouse_pos_;

  float pan_speed_ = 0.1f;
  float zoom_speed_ = 1.0f;

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4 override;

  void on_input_event(const Event& e, const InputStates& states) override;
  void on_mouse_move(MouseMoveEvent event, const InputStates& states);
  void on_mouse_scroll(MouseWheelEvent event);

  void draw_gui() override;

  [[nodiscard]] auto forward_axis() const -> beyond::Vec3;
  [[nodiscard]] auto right_axis() const -> beyond::Vec3;

  void reset() override;
};

} // namespace charlie

#endif // CHARLIE3D_CAMERA_HPP
