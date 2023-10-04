#pragma once

#include <vulkan/vulkan.h>

#include "context.hpp"
#include "error_handling.hpp"
#include <beyond/utils/zstring_view.hpp>

namespace vkh {

struct CommandBufferAllocInfo {
  VkCommandPool command_pool{};
  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto allocate_command_buffer(Context& context, CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>;

} // namespace vkh
