#pragma once

#include "error_handling.hpp"
#include "vulkan/vulkan_core.h"

#include <beyond/utils/zstring_view.hpp>
#include <span>

namespace vkh {

struct DescriptorSetLayoutCreateInfo {
  VkDescriptorSetLayoutCreateFlags flags = 0;
  std::span<const VkDescriptorSetLayoutBinding> bindings;
  beyond::ZStringView debug_name;
};

auto create_descriptor_set_layout(VkDevice device, const DescriptorSetLayoutCreateInfo& create_info)
    -> Expected<VkDescriptorSetLayout>;

} // namespace vkh