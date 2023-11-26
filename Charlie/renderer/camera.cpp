#include "camera.hpp"

#include <beyond/math/constants.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/utils/narrowing.hpp>

#include <imgui.h>

#include <SDL2/SDL.h>

namespace charlie {

[[nodiscard]] auto Camera::view_matrix() const -> Mat4
{
  return controller_->view_matrix();
}

[[nodiscard]] auto Camera::proj_matrix() const -> Mat4
{
  return beyond::perspective(fovy, aspect_ratio, z_near, z_far);
}

static void imgui_slider_degree(const char* label, beyond::Radian* v, beyond::Degree v_min,
                                beyond::Degree v_max, const char* format = "%.0f",
                                ImGuiSliderFlags flags = 0)
{
  auto val = beyond::to_degree(*v).value();
  ImGui::SliderFloat(label, &val, v_min.value(), v_max.value(), format, flags);
  *v = beyond::Degree{val};
}

void Camera::draw_gui()
{
  controller_->draw_gui();

  ImGui::SeparatorText("Perspective projection:");

  imgui_slider_degree("Field of view", &fovy, beyond::Degree{10}, beyond::Degree{90});

  ImGui::LabelText("Aspect ratio", "%f", aspect_ratio);
  ImGui::LabelText("Z near", "%f", z_near);
  ImGui::LabelText("Z far", "%f", z_far);

  if (ImGui::Button("Reset camera")) {
    fovy = beyond::Degree{70};

    controller_->reset();
  }
}

void Camera::fixed_update()
{
  controller_->fixed_update();
}

void Camera::on_input_event(const Event& event, const InputStates& states)
{
  std::visit(
      [&](const auto& e) {
        using EventType = std::remove_cvref_t<decltype(e)>;
        if constexpr (std::is_same_v<EventType, charlie::WindowEvent>) {
          if (e.type == charlie::WindowEventType::resize) {
            SDL_Window* window = SDL_GetWindowFromID(e.window_id);
            int width, height;
            SDL_GetWindowSize(window, &width, &height);
            aspect_ratio = beyond::narrow<f32>(width) / beyond::narrow<f32>(height);
          }
        }
      },
      event);

  controller_->on_input_event(event, states);
}

auto Camera::position() const -> beyond::Vec3
{
  return controller_->position();
}

void FirstPersonCameraController::fixed_update()
{
  const auto velocity = input_axis_;
  position_ += velocity * 0.1f;
}

auto FirstPersonCameraController::view_matrix() const -> beyond::Mat4
{
  auto camera_mat = beyond::translate(position_) *
                    beyond::look_at(Vec3{}, Vec3{0.0, 0.0, 1.0}, Vec3{0.0, 1.0, 0.0});

  return camera_mat;
}

void FirstPersonCameraController::on_key_input(int /*key*/, int /*scancode*/, int /*action*/,
                                               int /*mods*/)
{
  //  switch (action) {
  //  case GLFW_PRESS:
  //    switch (key) {
  //    case GLFW_KEY_W:
  //      input_axis_.z -= 1.f;
  //      break;
  //    case GLFW_KEY_S:
  //      input_axis_.z += 1.f;
  //      break;
  //    case GLFW_KEY_A:
  //      input_axis_.x -= 1.f;
  //      break;
  //    case GLFW_KEY_D:
  //      input_axis_.x += 1.f;
  //      break;
  //    case GLFW_KEY_R:
  //      input_axis_.y += 1.f;
  //      break;
  //    case GLFW_KEY_F:
  //      input_axis_.y -= 1.f;
  //      break;
  //    default:
  //      break;
  //    }
  //    break;
  //  case GLFW_RELEASE:
  //    switch (key) {
  //    case GLFW_KEY_W:
  //      input_axis_.z += 1.f;
  //      break;
  //    case GLFW_KEY_S:
  //      input_axis_.z -= 1.f;
  //      break;
  //    case GLFW_KEY_A:
  //      input_axis_.x += 1.f;
  //      break;
  //    case GLFW_KEY_D:
  //      input_axis_.x -= 1.f;
  //      break;
  //    case GLFW_KEY_R:
  //      input_axis_.y -= 1.f;
  //      break;
  //    case GLFW_KEY_F:
  //      input_axis_.y += 1.f;
  //      break;
  //    default:
  //      break;
  //    }
  //    break;
  //  default:
  //    break;
  //  }
}

auto ArcballCameraController::view_matrix() const -> beyond::Mat4
{
  const auto eye = eye_position_from_zooming(zooming_);
  return beyond::look_at(eye, lookat_, up_);
}

[[nodiscard]] auto ArcballCameraController::right_axis() const -> beyond::Vec3
{
  return cross(up_, forward_axis_);
}

void ArcballCameraController::on_mouse_move(MouseMoveEvent event, const InputStates& states)
{
  const auto mouse_pos = IPoint2{event.x, event.y};
  const auto delta_mouse = old_mouse_pos_ - mouse_pos;

  const auto [width, height] = window_->resolution();
  const auto normalized_delta_mouse = Vec2{
      narrow<f32>(delta_mouse.x) / narrow<f32>(width) * 2.0f,
      narrow<f32>(delta_mouse.y) / narrow<f32>(height),
  };

  const auto eye = eye_position_from_zooming(zooming_);

  if (states.mouse_button_down(MouseButton::left)) {

    constexpr auto pi = beyond::float_constants::pi;
    const auto delta_angle_x = normalized_delta_mouse.x * pi;
    auto delta_angle_y = normalized_delta_mouse.y * pi;

    // If the camera view direction is the same as up vector
    const f32 y_sign = delta_angle_y == 0.f ? 0.f : delta_angle_y > 0.f ? 1.f : -1.f;
    if (dot(forward_axis_, up_) * y_sign < -0.99f) { delta_angle_y = 0; }

    const Vec4 pivot{lookat_, 1};
    Vec4 position{eye, 1};
    const auto rotation_x = beyond::rotate(beyond::Radian{delta_angle_x}, up_);
    position = (rotation_x * (position - pivot)) + pivot;

    const auto rotation_y = beyond::rotate(beyond::Radian{delta_angle_y}, right_axis());
    position = (rotation_y * (position - pivot)) + pivot;

    // update camera position
    const Point3 new_eye = position.xyz;
    forward_axis_ = normalize(lookat_ - new_eye);
  }

  // Panning
  if (states.mouse_button_down(MouseButton::right)) {
    const Vec2 delta_pan = normalized_delta_mouse * zooming_;

    const auto delta = cross(right_axis(), forward_axis_) * delta_pan.y * pan_speed_ -
                       right_axis() * delta_pan.x * pan_speed_;

    desired_lookat_ += delta;
    if (!smooth_movement_) { lookat_ = desired_lookat_; }
  }

  old_mouse_pos_ = mouse_pos;
}

void ArcballCameraController::draw_gui()
{
  ImGui::SeparatorText("Arcball Controller:");
  ImGui::Checkbox("Smooth movement", &smooth_movement_);

  const auto eye = eye_position_from_zooming(zooming_);
  ImGui::LabelText("Position", "%f %f %f", eye.x, eye.y, eye.z);
  ImGui::LabelText("Look at", "%f %f %f", lookat_.x, lookat_.y, lookat_.z);

  ImGui::SliderFloat("Pan speed", &pan_speed_, 0, 10, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("Zoom speed", &zoom_speed_, 0, 100, "%.3f", ImGuiSliderFlags_Logarithmic);
}

void ArcballCameraController::reset()
{
  forward_axis_ = initial_forward_axis_;
  desired_lookat_ = initial_lookat_;
  pan_speed_ = initial_pan_speed;
  zoom_speed_ = initial_zoom_speed;
  desired_zooming_ = initial_zooming_;

  if (not smooth_movement_) {
    lookat_ = desired_lookat_;
    zooming_ = desired_zooming_;
  }
}

void ArcballCameraController::on_input_event(const Event& event, const InputStates& states)
{
  std::visit(
      [&](const auto& e) {
        using EventType = std::remove_cvref_t<decltype(e)>;
        if constexpr (std::is_same_v<EventType, MouseWheelEvent>) {
          on_mouse_scroll(e);
        } else if constexpr (std::is_same_v<EventType, MouseMoveEvent>) {
          on_mouse_move(e, states);
        }
      },
      event);
}

void ArcballCameraController::on_mouse_scroll(MouseWheelEvent event)
{
  desired_zooming_ -= event.y * zoom_speed_;
  static constexpr f32 min_zooming = 0.1f;
  desired_zooming_ = std::max(min_zooming, desired_zooming_);

  if (!smooth_movement_) { zooming_ = desired_zooming_; }
}

void ArcballCameraController::fixed_update()
{
  if (smooth_movement_) {
    static constexpr f32 transition_speed = 0.2f;

    zooming_ = beyond::lerp(zooming_, desired_zooming_, transition_speed);
    lookat_ = beyond::lerp(lookat_, desired_lookat_, transition_speed);
  }
}

[[nodiscard]] auto ArcballCameraController::eye_position_from_zooming(f32 zooming) const -> Point3
{
  return lookat_ - zooming * forward_axis_;
}

} // namespace charlie