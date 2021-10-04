add_subdirectory(beyond-core)

include(FetchContent)
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG 95b82a95378c2900f7d1d7908cee322b5f3bf120
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

## vulkan memory allocator
#add_library(vma INTERFACE)
#target_compile_definitions(vma INTERFACE
#        VMA_STATIC_VULKAN_FUNCTIONS=1)
#target_include_directories(vma SYSTEM
#        INTERFACE
#        $<INSTALL_INTERFACE:VulkanMemoryAllocator/include>
#        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include>
#        )
#add_library(third_party::vma ALIAS vma)
#if (CHARLIE3D_ENABLE_PCH)
#    target_precompile_headers(vma INTERFACE
#            "${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/src/vk_mem_alloc.h"
#            )
#endif ()
