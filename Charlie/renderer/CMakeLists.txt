add_library(charlie3d_renderer
        renderer.hpp renderer.cpp
        mesh.hpp mesh.cpp
        camera.cpp camera.hpp
        deletion_queue.hpp
        render_pass.hpp
        imgui_render_pass.cpp
        imgui_render_pass.hpp
        imgui_impl_vulkan.h
        imgui_impl_vulkan.cpp
        descriptor_allocator.hpp
        descriptor_allocator.cpp
        uploader.cpp uploader.hpp
        scene.cpp scene.hpp
        pipeline_manager.cpp
        pipeline_manager.hpp)
add_library(charlie3d::renderer ALIAS charlie3d_renderer)

find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)


target_link_libraries(charlie3d_renderer
        PUBLIC
        fmt::fmt
        charlie3d::vulkan_helper
        charlie3d::utils
        charlie3d::asset
        charlie3d::shader_compiler
        beyond::core
        imgui::imgui
        PRIVATE
        SDL2::SDL2
        charlie3d::compiler_options
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