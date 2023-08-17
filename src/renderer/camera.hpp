#ifndef CHARLIE3D_CAMERA_HPP
#define CHARLIE3D_CAMERA_HPP

#include <beyond/math/angle.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/math/point.hpp>
#include <beyond/math/quat.hpp>

namespace charlie {

struct CameraController;

class Camera {
public:
  beyond::Radian fovy = beyond::Degree{70};
  float aspect_ratio = 1.0f;
  float z_near = 0.1f;
  float z_far = 200.f;

private:
  CameraController* controller_ = nullptr;

public:
  explicit Camera(CameraController& controller) : controller_{&controller} {}

  auto set_controller(CameraController& controller)
  {
    controller_ = &controller;
  }

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4;
  [[nodiscard]] auto proj_matrix() const -> beyond::Mat4;

  void process_key_input(int key, int scancode, int action, int mods);

  void update();
};

struct CameraController {
  CameraController() = default;
  virtual ~CameraController() = default;
  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;

  virtual void update() {}
  [[nodiscard]] virtual auto view_matrix() const -> beyond::Mat4 = 0;
  virtual void process_key_input(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/) {}
};

class FirstPersonCameraController : public CameraController {
  beyond::Point3 position_{0.f, 0.f, 0.f};
  beyond::Vec3 input_axis_{0.f, 0.f, 0.f};

  void update() override;
  [[nodiscard]] auto view_matrix() const -> beyond::Mat4 override;
  void process_key_input(int key, int scancode, int action, int mods) override;
};

class ArcballCameraController : public CameraController {
  beyond::Point3 eye_ = beyond::Point3{0, 0, -1};   // camera position
  beyond::Point3 center_ = beyond::Point3{0, 0, 0}; // the point look at
  beyond::Vec3 up_ = beyond::Vec3{0, 1, 0};

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4 override;
};

} // namespace charlie

#endif // CHARLIE3D_CAMERA_HPP
