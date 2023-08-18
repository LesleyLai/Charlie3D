#include "camera.hpp"

#include <beyond/math/constants.hpp>
#include <beyond/math/transform.hpp>

#include <imgui.h>

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

void Camera::draw_gui()
{
  controller_->draw_gui();
}

void Camera::on_key_input(int key, int scancode, int action, int mods)
{
  controller_->on_key_input(key, scancode, action, mods);
}

void Camera::on_mouse_move(GLFWwindow* window, int x, int y)
{
  controller_->on_mouse_move(window, x, y);
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

void FirstPersonCameraController::on_key_input(int key, int /*scancode*/, int action, int /*mods*/)
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

void ArcballCameraController::on_mouse_move(GLFWwindow* window, int x, int y)
{
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
    const auto delta_mouse = old_mouse_pos_ - beyond::IPoint2{x, y};

    int width{}, height{};
    glfwGetWindowSize(window, &width, &height);

    constexpr auto pi = beyond::float_constants::pi;
    const auto delta_angle_x =
        static_cast<float>(delta_mouse.x) / static_cast<float>(width) * (2 * pi);
    auto delta_angle_y = static_cast<float>(delta_mouse.y) / static_cast<float>(height) * pi;

    const beyond::Vec4 pivot{center_, 1};
    beyond::Vec4 position{eye_, 1};

    const auto view_direction = normalize(center_ - eye_);

    // If the camera view direction is the same as up vector
    const float y_sign = delta_angle_y == 0.f ? 0.f : delta_angle_y > 0.f ? 1.f : -1.f;
    if (dot(view_direction, up_) * y_sign < -0.99f) { delta_angle_y = 0; }

    const auto rotation_x = beyond::rotate(beyond::Radian{delta_angle_x}, up_);
    position = (rotation_x * (position - pivot)) + pivot;

    const auto right_axis = normalize(cross(eye_ - center_, up_));
    const auto rotation_y = beyond::rotate(beyond::Radian{delta_angle_y}, right_axis);
    position = (rotation_y * (position - pivot)) + pivot;

    // update camera position
    eye_ = position.xyz;
  }

  old_mouse_pos_ = beyond::IPoint2{x, y};
}
void ArcballCameraController::draw_gui()
{
  ImGui::SeparatorText("Arcball Camera:");
  ImGui::Text("eye: %f %f %f", eye_.x, eye_.y, eye_.z);
  ImGui::Text("center: %f %f %f", center_.x, center_.y, center_.z);
  ImGui::Text("up: %f %f %f", up_.x, up_.y, up_.z);
}

} // namespace charlie