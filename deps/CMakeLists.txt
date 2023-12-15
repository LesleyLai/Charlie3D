add_subdirectory(beyond-core)

include("../cmake/CPM.cmake")

CPMAddPackage(
        NAME vk-bootstrap
        GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
        GIT_TAG v1.3.273
)

CPMAddPackage(
        NAME Vulkan-Headers
        GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
        GIT_TAG v1.3.273
        OVERRIDE_FIND_PACKAGE
)

CPMAddPackage("gh:KhronosGroup/Vulkan-Utility-Libraries@1.3.273")

CPMAddPackage(
        NAME SPIRV-Tools
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
        GIT_TAG vulkan-sdk-1.3.268.0
        OPTIONS
        "SKIP_SPIRV_TOOLS_INSTALL ON"
        "SPIRV_SKIP_EXECUTABLES ON"
        "SPIRV_SKIP_TESTS ON"
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
)