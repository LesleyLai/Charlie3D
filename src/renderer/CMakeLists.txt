add_subdirectory(vulkan_helpers)

add_library(charlie3d_renderer
        renderer.hpp
        renderer.cpp)
add_library(charlie3d::renderer ALIAS charlie3d_renderer)

target_link_libraries(charlie3d_renderer
        PUBLIC
        fmt::fmt
        charlie3d::vulkan_helper
        beyond::core
        )

include(../../cmake/CompileShader.cmake)
compile_shader(triangle_vert_shader
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/triangle.vert.spv
        )

compile_shader(triangle_frag_shader
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.glsl
        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/triangle.frag.spv
        )

add_dependencies(charlie3d_renderer triangle_vert_shader)
add_dependencies(charlie3d_renderer triangle_frag_shader)