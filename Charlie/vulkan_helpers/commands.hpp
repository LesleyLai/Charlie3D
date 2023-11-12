#pragma once

#include <vulkan/vulkan.h>

#include "beyond/utils/zstring_view.hpp"
#include "context.hpp"
#include "error_handling.hpp"

namespace vkh {

struct CommandBufferAllocInfo {
  VkCommandPool command_pool{};
  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto allocate_command_buffer(VkDevice device, CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>;

} // namespace vkh
