#pragma once

#include <span>

#include <vulkan/vulkan_core.h>

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

struct PipelineLayoutCreateInfo {
  const void* p_next = nullptr;
  VkPipelineLayoutCreateFlags flags = 0;
  std::span<const VkDescriptorSetLayout> set_layouts;
  std::span<const VkPushConstantRange> push_constant_ranges;
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto
create_pipeline_layout(VkDevice device,
                       const PipelineLayoutCreateInfo& create_info) -> Expected<VkPipelineLayout>;

struct CommandPoolCreateInfo {
  VkCommandPoolCreateFlags flags = 0;
  std::uint32_t queue_family_index = 0;
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto
create_command_pool(VkDevice device, CommandPoolCreateInfo create_info) -> Expected<VkCommandPool>;

struct CommandBufferAllocInfo {
  VkCommandPool command_pool{};
  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto allocate_command_buffer(VkDevice device, CommandBufferAllocInfo alloc_info)
    -> Expected<VkCommandBuffer>;

struct DescriptorSetLayoutCreateInfo {
  void* p_next = nullptr;
  VkDescriptorSetLayoutCreateFlags flags = 0;
  std::span<const VkDescriptorSetLayoutBinding> bindings;
  beyond::ZStringView debug_name;
};

auto create_descriptor_set_layout(VkDevice device, const DescriptorSetLayoutCreateInfo& create_info)
    -> Expected<VkDescriptorSetLayout>;

struct DescriptorPoolCreateInfo {
  VkDescriptorPoolCreateFlags flags = 0;
  std::uint32_t max_sets = 0;
  std::span<const VkDescriptorPoolSize> pool_sizes;
  beyond::ZStringView debug_name;
};

auto create_descriptor_pool(VkDevice device, const DescriptorPoolCreateInfo& create_info)
    -> Expected<VkDescriptorPool>;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct SubresourceRange {
  RequiredField<VkImageAspectFlags> aspect_mask;
  uint32_t base_mip_level = 0;
  uint32_t level_count = 1;
  uint32_t base_array_layer = 0;
  uint32_t layer_count = 1;

  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit(false) operator VkImageSubresourceRange() const
  {
    return VkImageSubresourceRange{
        .aspectMask = this->aspect_mask.value,
        .baseMipLevel = base_mip_level,
        .levelCount = level_count,
        .baseArrayLayer = base_array_layer,
        .layerCount = layer_count,
    };
  }
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct ImageViewCreateInfo {
  VkImageViewCreateFlags flags = 0;
  RequiredField<VkImage> image;
  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  RequiredField<VkFormat> format;
  VkComponentMapping components = {};
  RequiredField<SubresourceRange> subresource_range;
  beyond::ZStringView debug_name;
};

auto create_image_view(VkDevice device,
                       const ImageViewCreateInfo& image_view_create_info) -> Expected<VkImageView>;

struct ShaderModuleCreateInfo {
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto load_shader_module(VkDevice device, std::span<const uint32_t> buffer,
                                      const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>;

struct FenceCreateInfo {
  VkFenceCreateFlags flags = {};
  beyond::ZStringView debug_name;
};

struct SemaphoreCreateInfo {
  beyond::ZStringView debug_name;
};

[[nodiscard]] auto create_fence(VkDevice device,
                                const FenceCreateInfo& create_info) -> Expected<VkFence>;

[[nodiscard]] auto
create_semaphore(VkDevice device, const SemaphoreCreateInfo& create_info) -> Expected<VkSemaphore>;

} // namespace vkh
