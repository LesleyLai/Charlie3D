find_package(fmt REQUIRED)

add_subdirectory(window)
add_subdirectory(renderer)

add_executable(charlie3d "main.cpp")
target_link_libraries(charlie3d PRIVATE charlie3d::window charlie3d::renderer)

add_custom_target(assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/bin/assets
        )
add_dependencies(charlie3d assets)