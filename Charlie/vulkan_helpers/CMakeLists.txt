find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

add_library(vulkan_helper
        buffer.hpp
        context.cpp
        context.hpp
        debug_utils.hpp
        debug_utils.cpp
        error_handling.hpp
        graphics_pipeline.hpp
        swapchain.hpp
        swapchain.cpp
        header_only_impl.cpp
        image.hpp
        required_field.hpp
        error_handling.cpp initializers.cpp initializers.hpp)
target_link_libraries(vulkan_helper
        PUBLIC
        charlie3d::compiler_options
        fmt::fmt
        vk-bootstrap::vk-bootstrap
        Vulkan::Headers
        Vulkan::UtilityHeaders
        GPUOpen::VulkanMemoryAllocator
        volk::volk
        PRIVATE
        charlie3d::window
        spdlog::spdlog
        )
target_compile_definitions(vulkan_helper
        PUBLIC
        VK_NO_PROTOTYPES
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=0)

add_library(charlie3d::vulkan_helper ALIAS vulkan_helper)
