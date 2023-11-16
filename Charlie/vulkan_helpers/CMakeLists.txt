find_package(Vulkan REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)
find_package(spdlog REQUIRED)
find_package(volk CONFIG REQUIRED)

add_library(vulkan_helper
        buffer.hpp
        buffer.cpp
        commands.cpp
        commands.hpp
        context.cpp
        context.hpp
        debug_utils.hpp
        debug_utils.cpp
        deletion_queue.hpp
        descriptor_pool.hpp
        descriptor_pool.cpp
        error_handling.hpp
        graphics_pipeline.hpp
        shader_module.hpp
        shader_module.cpp
        swapchain.hpp
        swapchain.cpp
        sync.hpp
        sync.cpp
        vma_impl.cpp
        image.cpp
        image.hpp
        required_field.hpp
        image_view.cpp
        image_view.hpp
        error_handling.cpp descriptor_set_layout.cpp descriptor_set_layout.hpp)
target_link_libraries(vulkan_helper
        PUBLIC
        charlie3d::compiler_options
        fmt::fmt
        vk-bootstrap::vk-bootstrap
        #Vulkan::Vulkan
        vulkan-memory-allocator::vulkan-memory-allocator
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
