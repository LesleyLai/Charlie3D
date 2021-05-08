#pragma once

#include <beyond/utils/copy_move.hpp>

class Window;

class WindowManager {
public:
  [[nodiscard]] static auto instance() -> WindowManager&;

  void pull_events();

  /// @brief Creates a new window
  [[nodiscard]] auto create(int width, int height, const char* title) -> Window;

  ~WindowManager();
  BEYOND_DELETE_COPY(WindowManager)
  BEYOND_DELETE_MOVE(WindowManager)

private:
  WindowManager();
};