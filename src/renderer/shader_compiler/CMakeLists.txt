find_package(spdlog REQUIRED)

add_library(charlie3d_shader_compiler
        shader_compiler.hpp
        shader_compiler.cpp
        )
add_library(charlie3d::shader_compiler ALIAS charlie3d_shader_compiler)

target_link_libraries(charlie3d_shader_compiler
        PRIVATE
        charlie3d::compiler_options
        charlie3d::utils
        beyond::core
        shaderc
        )