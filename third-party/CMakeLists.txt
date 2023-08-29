add_subdirectory(beyond-core)

include(FetchContent)
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG 61f77612c70dd49a59157fe139a7d248a90e206a
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

set(VULKAN_SDK_VERSION sdk-1.3.261.0)

FetchContent_Declare(SPIRV-Headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG ${VULKAN_SDK_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
set(SPIRV_HEADERS_SKIP_INSTALL ON)
FetchContent_MakeAvailable(SPIRV-Headers)

FetchContent_Declare(SPIRV-Tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG ${VULKAN_SDK_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
set(SKIP_SPIRV_TOOLS_INSTALL ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_SKIP_TESTS ON)
FetchContent_MakeAvailable(SPIRV-Tools)

FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG ${VULKAN_SDK_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_SPVREMAPPER OFF)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(ENABLE_GLSLANG_BINARIES OFF)
FetchContent_MakeAvailable(glslang)
get_target_property(GLSL_SOURCE_DIR glslang SOURCE_DIR)

FetchContent_Declare(shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG v2023.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        )
set(SHADERC_ENABLE_SHARED_CRT OFF)
set(SHADERC_ENABLE_WGSL_OUTPUT OFF)
set(SHADERC_SKIP_INSTALL ON)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
set(SHADERC_GLSLANG_DIR ${GLSL_SOURCE_DIR})
FetchContent_MakeAvailable(shaderc)

add_subdirectory(imgui)
