#pragma once

#include <beyond/utils/utils.hpp>

#include <vulkan/vulkan_core.h>

#include <utility>

namespace vkh {

class Context;

[[nodiscard]] auto set_debug_name(Context& context, uint64_t object_handle,
                                  VkObjectType object_type, const char* name) noexcept -> VkResult;

template <typename VkHandle> [[nodiscard]] consteval auto get_object_type() -> VkObjectType
{
#define VKH_OBJ_TYPE_MAPPING(T, object_type)                                                       \
  if constexpr (std::same_as<VkHandle, T>) { return object_type; }

  VKH_OBJ_TYPE_MAPPING(VkInstance, VK_OBJECT_TYPE_INSTANCE);
  VKH_OBJ_TYPE_MAPPING(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);
  VKH_OBJ_TYPE_MAPPING(VkDevice, VK_OBJECT_TYPE_DEVICE);
  VKH_OBJ_TYPE_MAPPING(VkQueue, VK_OBJECT_TYPE_QUEUE);
  VKH_OBJ_TYPE_MAPPING(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE);
  VKH_OBJ_TYPE_MAPPING(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER);
  VKH_OBJ_TYPE_MAPPING(VkFence, VK_OBJECT_TYPE_FENCE);
  VKH_OBJ_TYPE_MAPPING(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  VKH_OBJ_TYPE_MAPPING(VkBuffer, VK_OBJECT_TYPE_BUFFER);
  VKH_OBJ_TYPE_MAPPING(VkImage, VK_OBJECT_TYPE_IMAGE);
  VKH_OBJ_TYPE_MAPPING(VkEvent, VK_OBJECT_TYPE_EVENT);
  VKH_OBJ_TYPE_MAPPING(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL);
  VKH_OBJ_TYPE_MAPPING(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW);
  VKH_OBJ_TYPE_MAPPING(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW);
  VKH_OBJ_TYPE_MAPPING(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW);
  VKH_OBJ_TYPE_MAPPING(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE);
  VKH_OBJ_TYPE_MAPPING(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE);
  VKH_OBJ_TYPE_MAPPING(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
  VKH_OBJ_TYPE_MAPPING(VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS);
  VKH_OBJ_TYPE_MAPPING(VkPipeline, VK_OBJECT_TYPE_PIPELINE);
  VKH_OBJ_TYPE_MAPPING(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
  VKH_OBJ_TYPE_MAPPING(VkSampler, VK_OBJECT_TYPE_SAMPLER);
  VKH_OBJ_TYPE_MAPPING(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET);
  VKH_OBJ_TYPE_MAPPING(VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);
  VKH_OBJ_TYPE_MAPPING(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL);
  VKH_OBJ_TYPE_MAPPING(VkSurfaceKHR, VK_OBJECT_TYPE_SURFACE_KHR);
  VKH_OBJ_TYPE_MAPPING(VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR);
  VKH_OBJ_TYPE_MAPPING(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);

#undef VKH_OBJ_TYPE_MAPPING
}

template <typename VkHandle>
[[nodiscard]] BEYOND_FORCE_INLINE auto set_debug_name(Context& context, VkHandle handle,
                                                      const char* name) noexcept -> VkResult
{
  return set_debug_name(context, std::bit_cast<uint64_t>(handle), get_object_type<VkHandle>(),
                        name);
}

void report_fail_to_set_debug_name(const char* name) noexcept;

} // namespace vkh
