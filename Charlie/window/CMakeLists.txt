find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(charlie3d_window
        window.hpp
        window.cpp
        window_manager.hpp
        window_manager.cpp input_handler.cpp input_handler.hpp resolution.hpp)
add_library(charlie3d::window ALIAS charlie3d_window)

target_link_libraries(charlie3d_window
        PUBLIC
        charlie3d::compiler_options
        charlie3d::utils
        fmt::fmt
        SDL2::SDL2
        beyond::core
        PRIVATE
        imgui::imgui
)

target_compile_definitions(charlie3d_window PUBLIC SDL_MAIN_HANDLED)