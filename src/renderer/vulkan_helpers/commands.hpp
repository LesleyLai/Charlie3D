#pragma once

#include <vulkan/vulkan.h>

#include "context.hpp"
#include "error_handling.hpp"

namespace vkh {

struct CommandBufferAllocInfo {
  VkCommandPool command_pool;
  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  const char* debug_name = nullptr;
};

[[nodiscard]] auto allocate_command_buffer(Context& context,
                                           CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>;

} // namespace vkh
