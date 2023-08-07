#pragma once

#include <vulkan/vulkan.h>

#include <span>
#include <string>

#include "error_handling.hpp"
#include "unique_resource.hpp"

namespace vkh {

class Context;

enum class PolygonMode {
  fill = VK_POLYGON_MODE_FILL,
  line = VK_POLYGON_MODE_LINE,
  point = VK_POLYGON_MODE_POINT,
};

enum class CullMode {
  none = VK_CULL_MODE_NONE,
  front = VK_CULL_MODE_FRONT_BIT,
  back = VK_CULL_MODE_BACK_BIT,
  front_and_back = VK_CULL_MODE_FRONT_AND_BACK,
};

struct PipelineVertexInputStateCreateInfo {
  std::span<const VkVertexInputBindingDescription> binding_descriptions;
  std::span<const VkVertexInputAttributeDescription> attribute_descriptions;
};

struct PipelineRenderingCreateInfo {
  uint32_t view_mask = 0;
  std::span<const VkFormat> color_attachment_formats;
  VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
  VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
};

template <typename T> struct RequiredField {
  T value;
  explicit(false) RequiredField(T v) : value{v} {}
};

struct GraphicsPipelineCreateInfo {
  RequiredField<VkPipelineLayout> layout;
  RequiredField<VkExtent2D> window_extend;
  RequiredField<PipelineRenderingCreateInfo> pipeline_rendering_create_info;

  // Optional
  const char* debug_name = nullptr;
  PipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
  std::span<const VkPipelineShaderStageCreateInfo> shader_stages;
  PolygonMode polygon_mode = PolygonMode::fill;
  CullMode cull_mode = CullMode::none;
};

[[nodiscard]] auto create_graphics_pipeline(Context& context,
                                            const GraphicsPipelineCreateInfo& create_info)
    -> Expected<VkPipeline>;

struct UniquePipeline : UniqueResource<VkPipeline, vkDestroyPipeline> {
  using UniqueResource<VkPipeline, vkDestroyPipeline>::UniqueResource;
};

[[nodiscard]] auto create_unique_graphics_pipeline(Context& context,
                                                   const GraphicsPipelineCreateInfo& create_info)
    -> Expected<UniquePipeline>;

} // namespace vkh
