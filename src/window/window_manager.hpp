#pragma once

#include <beyond/utils/copy_move.hpp>

namespace charlie {

class Window;

struct WindowOptions {
  bool resizable = false;
};

class WindowManager {
public:
  [[nodiscard]] static auto instance() -> WindowManager&;

  /// @brief Creates a new window
  [[nodiscard]] auto create(int width, int height, const char* title, const WindowOptions& options)
      -> Window;

  ~WindowManager();
  BEYOND_DELETE_COPY(WindowManager)

private:
  WindowManager();
};

} // namespace charlie