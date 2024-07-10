add_subdirectory(utils)
add_subdirectory(window)
add_subdirectory(asset)
add_subdirectory(shader_compiler)
add_subdirectory(vulkan_helpers)
add_subdirectory(renderer)

find_package(tinyfiledialogs REQUIRED)


add_executable(charlie3d "main.cpp"
        gui.cpp
        gui.hpp)
target_link_libraries(charlie3d
        PRIVATE
        charlie3d::window
        charlie3d::renderer
        charlie3d::compiler_options
        tinyfiledialogs::tinyfiledialogs)