find_package(Vulkan)

find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)

add_library(imgui_bindings
        bindings/imgui_impl_vulkan.cpp
        bindings/imgui_impl_vulkan.h
        bindings/imgui_impl_glfw.h
        bindings/imgui_impl_glfw.cpp
        )

target_link_libraries(imgui_bindings
        PUBLIC
        glfw::glfw
        imgui::imgui
        Vulkan::Vulkan)

target_include_directories(imgui_bindings
        PUBLIC
        )

target_include_directories(imgui_bindings
        SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings)


add_library(third-party::imgui_bindings ALIAS imgui_bindings)