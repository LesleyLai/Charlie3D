add_subdirectory(vulkan_helpers)
add_subdirectory(shader_compiler)

add_library(charlie3d_renderer
        renderer.hpp renderer.cpp
        mesh.hpp mesh.cpp
        camera.cpp camera.hpp
        render_pass.hpp
        imgui_render_pass.cpp
        imgui_render_pass.hpp
        uploader.cpp uploader.hpp
        scene.cpp scene.hpp
        )
add_library(charlie3d::renderer ALIAS charlie3d_renderer)

find_package(SDL2 REQUIRED)

target_link_libraries(charlie3d_renderer
        PUBLIC
        fmt::fmt
        charlie3d::vulkan_helper
        charlie3d::utils
        charlie3d::asset
        charlie3d::shader_compiler
        beyond::core
        third-party::imgui_bindings
        PRIVATE
        SDL2::SDL2-static
        )

#include(../../cmake/CompileShader.cmake)
##compile_shader(mesh_vert_shader
##        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert.glsl
##        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/mesh.vert.spv
##        )
#
#compile_shader(mesh_frag_shader
#        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.frag.glsl
#        TARGET ${CMAKE_BINARY_DIR}/bin/shaders/mesh.frag.spv
#        )

#add_dependencies(charlie3d_renderer mesh_vert_shader)
#add_dependencies(charlie3d_renderer mesh_frag_shader)