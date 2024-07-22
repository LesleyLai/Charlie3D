#pragma once

#include <vulkan/vulkan.h>

#include "beyond/utils/zstring_view.hpp"
#include <string>
#include <vector>

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

struct PipelineDepthStencilStateCreateInfo {
  const void* p_next = nullptr;
  VkPipelineDepthStencilStateCreateFlags flags = 0;
  VkBool32 depth_test_enable = VK_FALSE;
  VkBool32 depth_write_enable = VK_FALSE;
  VkCompareOp depth_compare_op = VK_COMPARE_OP_NEVER;
  VkBool32 depth_bounds_test_enable = VK_FALSE;
  VkBool32 stencil_test_enable = VK_FALSE;
  VkStencilOpState front = {};
  VkStencilOpState back = {};
  float min_depth_bounds = 0.0f;
  float max_depth_bounds = 1.0f;

  auto to_vk_struct() const -> VkPipelineDepthStencilStateCreateInfo
  {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = p_next,
        .flags = flags,
        .depthTestEnable = depth_test_enable,
        .depthWriteEnable = depth_write_enable,
        .depthCompareOp = depth_compare_op,
        .depthBoundsTestEnable = depth_bounds_test_enable,
        .stencilTestEnable = stencil_test_enable,
        .front = front,
        .back = back,
        .minDepthBounds = min_depth_bounds,
        .maxDepthBounds = max_depth_bounds,
    };
  }
};

} // namespace vkh
