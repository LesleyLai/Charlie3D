#pragma once

#include <vulkan/vulkan_core.h>

#include "required_field.hpp"

#include <span>

namespace vkh {

template <typename T> struct Transition {
  T src = {};
  T dst = {};
};

// wrapper of VkBufferMemoryBarrier2
struct BufferMemoryBarrier {
  Transition<VkPipelineStageFlags2> stage_masks;
  Transition<VkAccessFlags2> access_masks;
  Transition<uint32_t> queue_family_index = {VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED};
  RequiredField<VkBuffer> buffer;
  VkDeviceSize offset = 0;
  RequiredField<VkDeviceSize> size = 0;

  [[nodiscard]] auto to_vk_struct() const -> VkBufferMemoryBarrier2;
};

// wrapper of VkImageMemoryBarrier2
struct ImageBarrier { // NOLINT(cppcoreguidelines-pro-type-member-init)
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
  std::span<const VkMemoryBarrier2> memory_barriers;
  std::span<const VkBufferMemoryBarrier2> buffer_memory_barriers;
  std::span<const VkImageMemoryBarrier2> image_barriers;
};

// wrapper of vkCmdPipelineBarrier2
void cmd_pipeline_barrier(VkCommandBuffer command, const DependencyInfo& dependency_info);

} // namespace vkh