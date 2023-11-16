#pragma once

#include "error_handling.hpp"
#include "vulkan/vulkan_core.h"

#include <span>

namespace vkh {

struct DescriptorPoolCreateInfo {
  VkDescriptorPoolCreateFlags flags = 0;
  std::uint32_t max_sets = 0;
  std::span<const VkDescriptorPoolSize> pool_sizes;
  beyond::ZStringView debug_name;
};

auto create_descriptor_pool(VkDevice device, const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>;

} // namespace vkh
