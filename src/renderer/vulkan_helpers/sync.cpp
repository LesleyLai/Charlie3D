#include "sync.hpp"

#include "context.hpp"
#include "debug_utils.hpp"
#include "error_handling.hpp"

namespace vkh {

[[nodiscard]] auto create_fence(Context& context, const FenceCreateInfo& create_info)
    -> Expected<VkFence>
{
  const VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = create_info.flags,
  };

  VkFence fence = {};
  VKH_TRY(vkCreateFence(context, &fence_create_info, nullptr, &fence));

  if (set_debug_name(context, std::bit_cast<uint64_t>(fence), VK_OBJECT_TYPE_FENCE,
                     create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return fence;
}

[[nodiscard]] auto create_semaphore(Context& context, const SemaphoreCreateInfo& create_info)
    -> Expected<VkSemaphore>
{
  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  VkSemaphore semaphore = {};
  VKH_TRY(vkCreateSemaphore(context, &semaphore_create_info, nullptr, &semaphore));

  if (set_debug_name(context, std::bit_cast<uint64_t>(semaphore), VK_OBJECT_TYPE_SEMAPHORE,
                     create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return semaphore;
}

[[nodiscard]] auto ImageBarrier2::to_vk_struct() const -> VkImageMemoryBarrier2
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

void cmd_pipeline_barrier2(VkCommandBuffer command, const DependencyInfo& dependency_info)
{
  const VkDependencyInfo vk_dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = dependency_info.dependency_flags,
      .imageMemoryBarrierCount = beyond::narrow<uint32_t>(dependency_info.image_barriers.size()),
      .pImageMemoryBarriers = dependency_info.image_barriers.data(),
  };

  vkCmdPipelineBarrier2(command, &vk_dependency_info);
}

} // namespace vkh
