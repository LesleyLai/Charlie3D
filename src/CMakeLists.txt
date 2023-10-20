find_package(fmt REQUIRED)

add_subdirectory(utils)
add_subdirectory(window)
add_subdirectory(asset)
add_subdirectory(renderer)

add_executable(charlie3d "main.cpp")
target_link_libraries(charlie3d PRIVATE charlie3d::window charlie3d::renderer charlie3d::compiler_options)