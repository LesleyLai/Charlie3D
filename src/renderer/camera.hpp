#ifndef CHARLIE3D_CAMERA_HPP
#define CHARLIE3D_CAMERA_HPP

#include <beyond/math/angle.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/math/point.hpp>
#include <beyond/math/quat.hpp>

namespace charlie {

class Camera {
public:
  beyond::Radian fovy = beyond::Degree{70};
  beyond::Point3 position{0.f, 0.f, 0.f};
  beyond::Vec3 input_axis{0.f, 0.f, 0.f}; //

  float aspect_ratio = 1.0f;
  float z_near = 0.1f;
  float z_far = 200.f;

  [[nodiscard]] auto view_matrix() const -> beyond::Mat4;
  [[nodiscard]] auto proj_matrix() const -> beyond::Mat4;

  void process_key_input(int key, int scancode, int action, int mods);

  void update();
};

class CameraController {};

} // namespace charlie

#endif // CHARLIE3D_CAMERA_HPP
