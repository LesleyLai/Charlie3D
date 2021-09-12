#include "window_manager.hpp"
#include "window.hpp"

#include <GLFW/glfw3.h>

#include <beyond/utils/panic.hpp>

auto WindowManager::instance() -> WindowManager&
{
  static WindowManager s;
  return s;
}

WindowManager::WindowManager()
{
  if (!glfwInit()) { beyond::panic("Failed to initialize GLFW\n"); }
}

WindowManager::~WindowManager()
{
  glfwTerminate();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void WindowManager::pull_events()
{
  glfwPollEvents();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto WindowManager::create(int width, int height, const char* title) -> Window
{
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!window) { beyond::panic("Cannot create a glfw window"); }
  return Window{window};
}