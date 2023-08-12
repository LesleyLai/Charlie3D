#pragma once

#ifndef CHARLIE3D_IMAGE_VIEW_HPP
#define CHARLIE3D_IMAGE_VIEW_HPP

#include <vulkan/vulkan_core.h>

#include <span>

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

class Context;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct SubresourceRange {
  RequiredField<VkImageAspectFlags> aspect_mask;
  uint32_t base_mip_level = 0;
  uint32_t level_count = 1;
  uint32_t base_array_layer = 0;
  uint32_t layer_count = 1;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct ImageViewCreateInfo {
  VkImageViewCreateFlags flags = 0;
  RequiredField<VkImage> image;
  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  RequiredField<VkFormat> format;
  VkComponentMapping components = {};
  RequiredField<SubresourceRange> subresource_range;
  const char* debug_name = nullptr;
};

auto create_image_view(vkh::Context& context, const ImageViewCreateInfo& image_view_create_info)
    -> Expected<VkImageView>;

} // namespace vkh

#endif // CHARLIE3D_IMAGE_VIEW_HPP
