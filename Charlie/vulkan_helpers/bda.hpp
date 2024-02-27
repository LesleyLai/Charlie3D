#pragma once

#include <vulkan/vulkan.h>

namespace vkh {

// query a 64-bit buffer device address value
[[nodiscard]] auto get_buffer_device_address(VkDevice device, VkBuffer buffer) -> VkDeviceAddress;

} // namespace vkh