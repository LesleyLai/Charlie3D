#include "initializers.hpp"
#include "buffer.hpp"
#include "context.hpp"
#include "debug_utils.hpp"
#include "error_handling.hpp"
#include "image.hpp"

#include "../utils/prelude.hpp"
#include "beyond/utils/utils.hpp"
#include <volk.h>

namespace vkh {

[[nodiscard]] auto
create_pipeline_layout(VkDevice device,
                       const PipelineLayoutCreateInfo& create_info) -> Expected<VkPipelineLayout>
{
  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = create_info.p_next,
      .flags = create_info.flags,
      .setLayoutCount = beyond::narrow<beyond::u32>(create_info.set_layouts.size()),
      .pSetLayouts = create_info.set_layouts.data(),
      .pushConstantRangeCount =
          beyond::narrow<beyond::u32>(create_info.push_constant_ranges.size()),
      .pPushConstantRanges = create_info.push_constant_ranges.data(),
  };

  VkPipelineLayout layout = {};
  VKH_TRY(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &layout));
  VKH_TRY(vkh::set_debug_name(device, layout, create_info.debug_name));
  return layout;
}

[[nodiscard]] auto create_command_pool(VkDevice device,
                                       CommandPoolCreateInfo create_info) -> Expected<VkCommandPool>
{
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = create_info.flags,
      .queueFamilyIndex = create_info.queue_family_index};

  VkCommandPool command_pool{};
  VKH_TRY(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
  VKH_TRY(vkh::set_debug_name(device, command_pool, create_info.debug_name));
  return command_pool;
}

[[nodiscard]] auto allocate_command_buffer(VkDevice device, CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>
{
  const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, alloc_info.command_pool,
      alloc_info.level, 1};

  VkCommandBuffer command_buffer{};
  VKH_TRY(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer));
  VKH_TRY(vkh::set_debug_name(device, command_buffer, alloc_info.debug_name));
  return command_buffer;
}

auto create_descriptor_pool(VkDevice device, const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>
{

  const VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = create_info.flags,
      .maxSets = create_info.max_sets,
      .poolSizeCount = beyond::narrow<uint32_t>(create_info.pool_sizes.size()),
      .pPoolSizes = create_info.pool_sizes.data(),
  };

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VKH_TRY(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool));

  if (set_debug_name(device, descriptor_pool, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return descriptor_pool;
}

auto create_descriptor_set_layout(VkDevice device, const DescriptorSetLayoutCreateInfo& create_info)
    -> Expected<VkDescriptorSetLayout>
{
  VkDescriptorSetLayoutCreateInfo vk_create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = create_info.p_next,
      .flags = create_info.flags,
      .bindingCount = beyond::narrow<uint32_t>(create_info.bindings.size()),
      .pBindings = create_info.bindings.data(),
  };

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VKH_TRY(vkCreateDescriptorSetLayout(device, &vk_create_info, nullptr, &layout));

  if (set_debug_name(device, layout, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return layout;
}

auto create_image(vkh::Context& context,
                  const ImageCreateInfo& image_create_info) -> Expected<AllocatedImage>
{
  const VkImageCreateInfo vk_image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = image_create_info.flags,
      .imageType = image_create_info.image_type,
      .format = image_create_info.format.value,
      .extent = image_create_info.extent.value,
      .mipLevels = image_create_info.mip_levels,
      .arrayLayers = image_create_info.array_layers,
      .samples = image_create_info.samples,
      .tiling = image_create_info.tiling,
      .usage = image_create_info.usage.value,
      .sharingMode = image_create_info.sharing_mode,
      .queueFamilyIndexCount =
          beyond::narrow<uint32_t>(image_create_info.queue_fimily_indices.size()),
      .pQueueFamilyIndices = image_create_info.queue_fimily_indices.data(),
      .initialLayout = image_create_info.initial_layout,
  };

  static constexpr VmaAllocationCreateInfo image_alloc_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  AllocatedImage image;
  VKH_TRY(vmaCreateImage(context.allocator(), &vk_image_create_info, &image_alloc_info,
                         &image.image, &image.allocation, &image.allocation_info));

  if (set_debug_name(context, image.image, image_create_info.debug_name)) {
    report_fail_to_set_debug_name(image_create_info.debug_name);
  }

  return image;
}

void destroy_image(Context& context, AllocatedImage& image)
{
  vmaDestroyImage(context.allocator(), image.image, image.allocation);
}

auto create_image_view(VkDevice device,
                       const ImageViewCreateInfo& image_view_create_info) -> Expected<VkImageView>
{
  const VkImageViewCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                          .pNext = nullptr,
                                          .flags = image_view_create_info.flags,
                                          .image = image_view_create_info.image.value,
                                          .viewType = image_view_create_info.view_type,
                                          .format = image_view_create_info.format.value,
                                          .components = image_view_create_info.components,
                                          .subresourceRange =
                                              image_view_create_info.subresource_range.value};

  VkImageView image_view = VK_NULL_HANDLE;
  VKH_TRY(vkCreateImageView(device, &create_info, nullptr, &image_view));

  if (set_debug_name(device, image_view, image_view_create_info.debug_name)) {
    report_fail_to_set_debug_name(image_view_create_info.debug_name);
  }

  return image_view;
}

[[nodiscard]] auto load_shader_module(VkDevice device, std::span<const uint32_t> buffer,
                                      const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>
{
  const VkShaderModuleCreateInfo vk_create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = buffer.size_bytes(),
      .pCode = buffer.data(),
  };

  VkShaderModule shader_module = VK_NULL_HANDLE;
  VKH_TRY(vkCreateShaderModule(device, &vk_create_info, nullptr, &shader_module));

  if (set_debug_name(device, shader_module, create_info.debug_name)) {
    fmt::print("Cannot create debug name for {}\n", create_info.debug_name);
  }
  return shader_module;
}

[[nodiscard]] auto create_fence(VkDevice device,
                                const FenceCreateInfo& create_info) -> Expected<VkFence>
{
  const VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = create_info.flags,
  };

  VkFence fence = {};
  VKH_TRY(vkCreateFence(device, &fence_create_info, nullptr, &fence));

  if (set_debug_name(device, fence, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return fence;
}

[[nodiscard]] auto create_semaphore(VkDevice device,
                                    const SemaphoreCreateInfo& create_info) -> Expected<VkSemaphore>
{
  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };

  VkSemaphore semaphore = {};
  VKH_TRY(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphore));

  if (set_debug_name(device, semaphore, create_info.debug_name)) {
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
