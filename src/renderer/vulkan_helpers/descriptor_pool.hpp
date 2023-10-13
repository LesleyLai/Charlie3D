#pragma once

#include "../../utils/prelude.hpp"
#include "error_handling.hpp"
#include "vulkan/vulkan_core.h"

#include <span>

namespace vkh {

class Context;

struct DescriptorPoolCreateInfo {
  VkDescriptorPoolCreateFlags flags = 0;
  u32 max_sets = 0;
  std::span<const VkDescriptorPoolSize> pool_sizes;
  beyond::ZStringView debug_name;
};

auto create_descriptor_pool(Context& context, const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>;

} // namespace vkh
