#pragma once

#include <string_view>
#include <vulkan/vulkan.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

struct ShaderModuleCreateInfo {
  const char* debug_name = nullptr;
};

[[nodiscard]] auto
load_shader_module_from_file(Context& context, std::string_view filename,
                             const ShaderModuleCreateInfo& create_info)
    -> Expected<VkShaderModule>;

} // namespace vkh
