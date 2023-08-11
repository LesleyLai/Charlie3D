#include "graphics_pipeline.hpp"

#include "context.hpp"
#include "debug_utils.hpp"
#include "error_handling.hpp"

#include <beyond/utils/assert.hpp>
#include <beyond/utils/bit_cast.hpp>
#include <beyond/utils/conversion.hpp>

namespace vkh {

[[nodiscard]] auto create_graphics_pipeline(Context& context,
                                            const GraphicsPipelineCreateInfo& create_info)
    -> beyond::expected<VkPipeline, VkResult>
{
  BEYOND_ENSURE(create_info.layout.value != VK_NULL_HANDLE);

  using beyond::to_u32;

  const auto vertex_binding_descriptions =
      create_info.vertex_input_state_create_info.binding_descriptions;
  const auto vertex_attribute_descriptions =
      create_info.vertex_input_state_create_info.attribute_descriptions;

  const VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = to_u32(vertex_binding_descriptions.size()),
      .pVertexBindingDescriptions = vertex_binding_descriptions.data(),
      .vertexAttributeDescriptionCount = to_u32(vertex_attribute_descriptions.size()),
      .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()};

  static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  const VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = static_cast<VkPolygonMode>(create_info.polygon_mode),
      .cullMode = static_cast<VkCullModeFlags>(create_info.cull_mode),
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  static constexpr VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  static constexpr VkPipelineColorBlendStateCreateInfo color_blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  static constexpr VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  const auto pipeline_rendering_create_info = create_info.pipeline_rendering_create_info.value;
  const VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .viewMask = pipeline_rendering_create_info.view_mask,
      .colorAttachmentCount =
          to_u32(pipeline_rendering_create_info.color_attachment_formats.size()),
      .pColorAttachmentFormats = pipeline_rendering_create_info.color_attachment_formats.data(),
      .depthAttachmentFormat = pipeline_rendering_create_info.depth_attachment_format,
      .stencilAttachmentFormat = pipeline_rendering_create_info.stencil_attachment_format,
  };

  static constexpr VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                      VK_DYNAMIC_STATE_SCISSOR};

  static constexpr VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = beyond::size(dynamic_states),
      .pDynamicStates = dynamic_states};

  const VkGraphicsPipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &vk_pipeline_rendering_create_info,
      .stageCount = to_u32(create_info.shader_stages.size()),
      .pStages = create_info.shader_stages.data(),
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depth_stencil_state,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state_create_info,
      .layout = create_info.layout.value,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };

  VkPipeline pipeline{};
  VKH_TRY(vkCreateGraphicsPipelines(context.device(), VK_NULL_HANDLE, 1, &pipeline_create_info,
                                    nullptr, &pipeline));

  if (create_info.debug_name != nullptr &&
      set_debug_name(context, beyond::bit_cast<uint64_t>(pipeline), VK_OBJECT_TYPE_PIPELINE,
                     create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return pipeline;
}

[[nodiscard]] auto create_unique_graphics_pipeline(Context& context,
                                                   const GraphicsPipelineCreateInfo& create_info)
    -> Expected<UniquePipeline>
{
  return create_graphics_pipeline(context, create_info).map([&](VkPipeline pipeline) {
    return UniquePipeline{context.device(), pipeline};
  });
}

} // namespace vkh