#pragma once

#include <vulkan/vulkan_core.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

struct FenceCreateInfo {
  VkFenceCreateFlags flags = {};
  const char* debug_name = nullptr;
};

struct SemaphoreCreateInfo {
  const char* debug_name = nullptr;
};

[[nodiscard]] auto create_fence(Context& context, const FenceCreateInfo& create_info)
    -> Expected<VkFence>;

[[nodiscard]] auto create_semaphore(Context& context, const SemaphoreCreateInfo& create_info)
    -> Expected<VkSemaphore>;

} // namespace vkh
