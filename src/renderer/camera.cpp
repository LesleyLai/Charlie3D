#include "camera.hpp"

#include <beyond/math/serial.hpp>
#include <beyond/math/transform.hpp>

#include <glfw/glfw3.h>

namespace charlie {

[[nodiscard]] auto Camera::view_matrix() const -> beyond::Mat4
{
  return controller_->view_matrix();
}

[[nodiscard]] auto Camera::proj_matrix() const -> beyond::Mat4
{
  return beyond::perspective(fovy, aspect_ratio, z_near, z_far);
}

void Camera::process_key_input(int key, int scancode, int action, int mods)
{
  controller_->process_key_input(key, scancode, action, mods);
}

void Camera::update()
{
  controller_->update();
}

void FirstPersonCameraController::update()
{
  const auto velocity = input_axis_;
  position_ += velocity * 0.1f;
}

auto FirstPersonCameraController::view_matrix() const -> beyond::Mat4
{
  auto camera_mat =
      beyond::translate(position_) *
      beyond::look_at(beyond::Vec3{}, beyond::Vec3{0.0, 0.0, 1.0}, beyond::Vec3{0.0, 1.0, 0.0});

  return camera_mat;
}

void FirstPersonCameraController::process_key_input(int key, int /*scancode*/, int action,
                                                    int /*mods*/)
{
  switch (action) {
  case GLFW_PRESS:
    switch (key) {
    case GLFW_KEY_W:
      input_axis_.z -= 1.f;
      break;
    case GLFW_KEY_S:
      input_axis_.z += 1.f;
      break;
    case GLFW_KEY_A:
      input_axis_.x -= 1.f;
      break;
    case GLFW_KEY_D:
      input_axis_.x += 1.f;
      break;
    case GLFW_KEY_R:
      input_axis_.y += 1.f;
      break;
    case GLFW_KEY_F:
      input_axis_.y -= 1.f;
      break;
    default:
      break;
    }
    break;
  case GLFW_RELEASE:
    switch (key) {
    case GLFW_KEY_W:
      input_axis_.z += 1.f;
      break;
    case GLFW_KEY_S:
      input_axis_.z -= 1.f;
      break;
    case GLFW_KEY_A:
      input_axis_.x += 1.f;
      break;
    case GLFW_KEY_D:
      input_axis_.x -= 1.f;
      break;
    case GLFW_KEY_R:
      input_axis_.y -= 1.f;
      break;
    case GLFW_KEY_F:
      input_axis_.y += 1.f;
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

auto ArcballCameraController::view_matrix() const -> beyond::Mat4
{
  return beyond::look_at(eye_, center_, up_);
}
} // namespace charlie