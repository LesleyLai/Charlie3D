add_subdirectory(beyond-core)

include("../cmake/CPM.cmake")

cpmaddpackage(
        NAME volk
        GITHUB_REPOSITORY zeux/volk
        GIT_TAG vulkan-sdk-1.4.309.0
        OPTIONS
        "VOLK_INSTALL OFF"
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
        NAME vk-bootstrap
        GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
        GIT_TAG v1.4.312
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
        NAME Vulkan-Headers
        GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
        GIT_TAG v1.4.312
        OVERRIDE_FIND_PACKAGE
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
        NAME Vulkan-Utility-Libraries
        GITHUB_REPOSITORY KhronosGroup/Vulkan-Utility-Libraries
        GIT_TAG v1.4.312
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
        NAME SPIRV-Headers
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
        GIT_TAG vulkan-sdk-1.4.309.0
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
)

CPMAddPackage(
        NAME SPIRV-Tools
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
        GIT_TAG vulkan-sdk-1.4.309.0
        SYSTEM TRUE
        EXCLUDE_FROM_ALL TRUE
        OPTIONS
        "SKIP_SPIRV_TOOLS_INSTALL ON"
        "SPIRV_SKIP_EXECUTABLES ON"
        "SPIRV_SKIP_TESTS ON"
        "SPIRV_WERROR OFF"
)

CPMAddPackage(
        NAME glslang
        GITHUB_REPOSITORY KhronosGroup/glslang
        GIT_TAG 13.1.1
        OPTIONS
        "SKIP_GLSLANG_INSTALL ON"
        "ENABLE_SPVREMAPPER OFF"
        "ENABLE_GLSLANG_BINARIES OFF"
        "ENABLE_GLSLANG_JS OFF"
        "ENABLE_GLSLANG_BINARIES OFF"
)

CPMAddPackage(
        NAME shaderc
        GITHUB_REPOSITORY google/shaderc
        GIT_TAG v2023.7
        OPTIONS
        "SHADERC_SKIP_TESTS ON"
        "SHADERC_SKIP_EXAMPLES ON"
        "SHADERC_SKIP_INSTALL ON"
        "SHADERC_SKIP_COPYRIGHT_CHECK ON"
        "SHADERC_ENABLE_SHARED_CRT ON"
        "SHADERC_ENABLE_WERROR_COMPILE OFF"
)
