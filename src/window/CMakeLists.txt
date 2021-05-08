find_package(glfw3 REQUIRED)

add_library(charlie3d_window
        window.hpp
        window.cpp
        window_manager.hpp
        window_manager.cpp)
add_library(charlie3d::window ALIAS charlie3d_window)

target_link_libraries(charlie3d_window
        PUBLIC
        charlie3d::compiler_options
        fmt::fmt
        glfw::glfw
        beyond::core
        )