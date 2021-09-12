#include "window.hpp"

#include <beyond/utils/panic.hpp>

#include <GLFW/glfw3.h>

Window::~Window()
{
  glfwDestroyWindow(window_);
}

void Window::swap_buffers() noexcept
{
  glfwSwapBuffers(window_);
}

auto Window::should_close() const noexcept -> bool
{
  return glfwWindowShouldClose(window_);
}

[[nodiscard]] auto Window::resolution() const noexcept -> Resolution
{
  int width, height;
  glfwGetWindowSize(window_, &width, &height);
  return Resolution{static_cast<std::uint32_t>(width),
                    static_cast<std::uint32_t>(height)};
}