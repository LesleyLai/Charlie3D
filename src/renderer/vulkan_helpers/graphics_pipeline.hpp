#pragma once

#include <vulkan/vulkan.h>

#include <beyond/utils/zstring_view.hpp>
#include <string>
#include <vector>

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

struct PipelineVertexInputStateCreateInfo {
  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
};

struct PipelineRenderingCreateInfo {
  uint32_t view_mask = 0;
  std::vector<VkFormat> color_attachment_formats;
  VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
  VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
};

} // namespace vkh
