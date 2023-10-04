#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

struct ShaderModuleCreateInfo {
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto load_shader_module(Context& context, std::span<const uint32_t> buffer,
                                      const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>;

} // namespace vkh
