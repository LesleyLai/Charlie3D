#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

struct ShaderModuleCreateInfo {
  const char* debug_name = nullptr;
};

[[nodiscard]] auto load_shader_module(Context& context, std::span<const uint32_t> buffer,
                                      const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>;

} // namespace vkh
