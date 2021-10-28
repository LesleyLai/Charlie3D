add_subdirectory(vulkan_helpers)

add_library(charlie3d_renderer
        renderer.hpp
        renderer.cpp mesh.cpp mesh.hpp)
add_library(charlie3d::renderer ALIAS charlie3d_renderer)

find_package(assimp REQUIRED)
find_package(stb REQUIRED)

target_link_libraries(charlie3d_renderer
        PUBLIC
        fmt::fmt
        charlie3d::vulkan_helper
        beyond::core
        PRIVATE
        assimp::assimp
        stb::stb
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