#include "pipeline_barrier.hpp"

#include <volk.h>

#include "beyond/utils/narrowing.hpp"

namespace vkh {

[[nodiscard]] auto ImageBarrier::to_vk_struct() const -> VkImageMemoryBarrier2
{
  return VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                               .pNext = 0,
                               .srcStageMask = stage_masks.src,
                               .srcAccessMask = access_masks.src,
                               .dstStageMask = stage_masks.dst,
                               .dstAccessMask = access_masks.dst,
                               .oldLayout = layouts.src,
                               .newLayout = layouts.dst,
                               .srcQueueFamilyIndex = queue_family_index.src,
                               .dstQueueFamilyIndex = queue_family_index.dst,
                               .image = image.value,
                               .subresourceRange = subresource_range};
}

auto BufferMemoryBarrier::to_vk_struct() const -> VkBufferMemoryBarrier2
{
  return {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = stage_masks.src,
      .srcAccessMask = access_masks.src,
      .dstStageMask = stage_masks.dst,
      .dstAccessMask = access_masks.dst,
      .srcQueueFamilyIndex = queue_family_index.src,
      .dstQueueFamilyIndex = queue_family_index.dst,
      .buffer = buffer,
      .offset = offset,
      .size = size,
  };
}

void cmd_pipeline_barrier(VkCommandBuffer command, const DependencyInfo& dependency_info)
{
  const VkDependencyInfo vk_dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .dependencyFlags = dependency_info.dependency_flags,
      .memoryBarrierCount = beyond::narrow<uint32_t>(dependency_info.memory_barriers.size()),
      .pMemoryBarriers = dependency_info.memory_barriers.data(),
      .bufferMemoryBarrierCount =
          beyond::narrow<uint32_t>(dependency_info.buffer_memory_barriers.size()),
      .pBufferMemoryBarriers = dependency_info.buffer_memory_barriers.data(),
      .imageMemoryBarrierCount = beyond::narrow<uint32_t>(dependency_info.image_barriers.size()),
      .pImageMemoryBarriers = dependency_info.image_barriers.data(),
  };

  vkCmdPipelineBarrier2(command, &vk_dependency_info);
}

} // namespace vkh