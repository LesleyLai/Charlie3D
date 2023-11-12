#pragma once

#include <span>

#include <vulkan/vulkan_core.h>

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

struct FenceCreateInfo {
  VkFenceCreateFlags flags = {};
  beyond::ZStringView debug_name;
};

struct SemaphoreCreateInfo {
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto create_fence(VkDevice device, const FenceCreateInfo& create_info)
    -> Expected<VkFence>;

[[nodiscard]] auto create_semaphore(VkDevice device, const SemaphoreCreateInfo& create_info)
    -> Expected<VkSemaphore>;

template <typename T> struct Transition {
  T src = {};
  T dst = {};
};

struct ImageBarrier2 { // NOLINT(cppcoreguidelines-pro-type-member-init)
  Transition<VkPipelineStageFlags2> stage_masks;
  Transition<VkAccessFlagBits2> access_masks;
  Transition<VkImageLayout> layouts;
  Transition<uint32_t> queue_family_index = {VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED};

  RequiredField<VkImage> image;
  VkImageSubresourceRange subresource_range = {};

  [[nodiscard]] auto to_vk_struct() const -> VkImageMemoryBarrier2;
};

struct DependencyInfo {
  VkDependencyFlags dependency_flags = 0;
  std::span<const VkImageMemoryBarrier2> image_barriers;
};

void cmd_pipeline_barrier2(VkCommandBuffer command, const DependencyInfo& dependency_info);

} // namespace vkh
