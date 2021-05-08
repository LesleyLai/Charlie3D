add_library(charlie3d_renderer
        renderer.hpp
        renderer.cpp)
add_library(charlie3d::renderer ALIAS charlie3d_renderer)

target_link_libraries(charlie3d_renderer
        PUBLIC
        charlie3d::compiler_options
        fmt::fmt)