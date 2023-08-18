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

void Camera::on_mouse_scroll(float x, float y)
{
  controller_->on_mouse_scroll(x, y);
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

[[nodiscard]] auto ArcballCameraController::forward_axis() const -> beyond::Vec3
{
  return normalize(center_ - eye_);
}

[[nodiscard]] auto ArcballCameraController::right_axis() const -> beyond::Vec3
{
  return cross(up_, forward_axis());
}

void ArcballCameraController::on_mouse_move(GLFWwindow* window, int x, int y)
{
  const auto delta_mouse = old_mouse_pos_ - beyond::IPoint2{x, y};

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {

    int width{}, height{};
    glfwGetWindowSize(window, &width, &height);

    constexpr auto pi = beyond::float_constants::pi;
    const auto delta_angle_x =
        static_cast<float>(delta_mouse.x) / static_cast<float>(width) * (2 * pi);
    auto delta_angle_y = static_cast<float>(delta_mouse.y) / static_cast<float>(height) * pi;

    const beyond::Vec4 pivot{center_, 1};
    beyond::Vec4 position{eye_, 1};

    // If the camera view direction is the same as up vector
    const float y_sign = delta_angle_y == 0.f ? 0.f : delta_angle_y > 0.f ? 1.f : -1.f;
    if (dot(forward_axis(), up_) * y_sign < -0.99f) { delta_angle_y = 0; }

    const auto rotation_x = beyond::rotate(beyond::Radian{delta_angle_x}, up_);
    position = (rotation_x * (position - pivot)) + pivot;

    const auto rotation_y = beyond::rotate(beyond::Radian{delta_angle_y}, right_axis());
    position = (rotation_y * (position - pivot)) + pivot;

    // update camera position
    eye_ = position.xyz;
  }

  // Panning
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
    const auto delta =
        cross(forward_axis(), right_axis()) * static_cast<float>(delta_mouse.y) * pan_speed_ +
        right_axis() * static_cast<float>(delta_mouse.x) * pan_speed_;
    eye_ += delta;
    center_ += delta;
  }
  old_mouse_pos_ = beyond::IPoint2{x, y};
}

void ArcballCameraController::draw_gui()
{
  ImGui::SeparatorText("Arcball Camera:");

  ImGui::LabelText("Position", "%f %f %f", eye_.x, eye_.y, eye_.z);
  ImGui::LabelText("Look at", "%f %f %f", center_.x, center_.y, center_.z);

  ImGui::SliderFloat("Pan speed", &pan_speed_, 0, 10, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("Zoom speed", &zoom_speed_, 0, 10, "%.3f", ImGuiSliderFlags_Logarithmic);

  if (ImGui::Button("Reset camera")) { reset(); }
}

void ArcballCameraController::reset()
{
  eye_ = beyond::Point3{0, 0, -1};
  center_ = beyond::Point3{0, 0, 0};
  pan_speed_ = 0.1f;
  zoom_speed_ = 1.0f;
}

void ArcballCameraController::on_mouse_scroll(float /*x*/, float y)
{
  const auto zooming = (eye_ - center_).length();

  const auto advance_amount = forward_axis() * log(zooming + 1) * y * zoom_speed_;
  // Don't zoom in if too close
  if (advance_amount.length() <= zooming - 0.1 || y < 0) { eye_ += advance_amount; }
}

} // namespace charlie