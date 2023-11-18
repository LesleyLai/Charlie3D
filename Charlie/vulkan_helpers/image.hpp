#pragma once

#ifndef CHARLIE3D_VULKAN_HELPER_IMAGE_HPP
#define CHARLIE3D_VULKAN_HELPER_IMAGE_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <span>

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

class Context;

struct AllocatedImage {
  VkImage image = {};
  VmaAllocation allocation = {};

  explicit(false) operator VkImage() { return image; } // NOLINT(google-explicit-constructor)
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct ImageCreateInfo {
  RequiredField<VkFormat> format;
  RequiredField<VkExtent3D> extent;
  RequiredField<VkImageUsageFlags> usage;

  VkImageCreateFlags flags = 0;
  VkImageType image_type = VK_IMAGE_TYPE_2D;
  uint32_t mip_levels = 1;
  uint32_t array_layers = 1;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
  VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
  std::span<uint32_t> queue_fimily_indices = {};
  VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  beyond::ZStringView debug_name;
};

auto create_image(vkh::Context& context, const ImageCreateInfo& image_create_info)
    -> Expected<AllocatedImage>;

void destroy_image(vkh::Context& context, AllocatedImage& image);

} // namespace vkh

#endif // CHARLIE3D_VULKAN_HELPER_IMAGE_HPP
