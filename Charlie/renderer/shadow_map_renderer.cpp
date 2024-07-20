#include "shadow_map_renderer.hpp"
#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/debug_utils.hpp"
#include "../vulkan_helpers/initializers.hpp"
#include "renderer.hpp"
#include "sampler_cache.hpp"

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace charlie {

ShadowMapRenderer::ShadowMapRenderer(Renderer& renderer, Ref<SamplerCache> sampler_cache)
    : renderer_{renderer}
{
  ZoneScoped;

  vkh::Context& context = renderer.context();

  shadow_map_image_ =
      vkh::create_image(
          context,
          vkh::ImageCreateInfo{
              .format = shadow_map_format_,
              .extent = VkExtent3D{shadow_map_width_, shadow_map_height_, 1},
              .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              .debug_name = "Shadow Map Image",
          })
          .expect("Fail to create shadow map image");
  shadow_map_image_view_ =
      vkh::create_image_view(
          context, vkh::ImageViewCreateInfo{.image = shadow_map_image_.image,
                                            .format = shadow_map_format_,
                                            .subresource_range =
                                                vkh::SubresourceRange{
                                                    .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                },
                                            .debug_name = "Shadow Map Image View"})
          .expect("Fail to create shadow map image view");

  const VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                            .magFilter = VK_FILTER_LINEAR,
                                            .minFilter = VK_FILTER_LINEAR,
                                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
  shadow_map_sampler_ = sampler_cache->create_sampler(sampler_info);
  VK_CHECK(vkh::set_debug_name(context, shadow_map_sampler_, "Shadow Map Sampler"));
}

ShadowMapRenderer::~ShadowMapRenderer()
{
  vkh::Context& context = renderer_.context();

  vkDestroyPipelineLayout(context, shadow_map_pipeline_layout_, nullptr);

  vkDestroyImageView(context, shadow_map_image_view_, nullptr);
  vkh::destroy_image(context, shadow_map_image_);
}

void ShadowMapRenderer::init_pipeline()
{
  vkh::Context& context = renderer_.context();

  ZoneScoped;

  const ShaderHandle vertex_shader =
      renderer_.pipeline_manager().add_shader("shadow.vert.glsl", ShaderStage::vertex);

  const VkDescriptorSetLayout set_layouts[] = {renderer_.global_descriptor_set_layout,
                                               renderer_.object_descriptor_set_layout};

  shadow_map_pipeline_layout_ = vkh::create_pipeline_layout(context,
                                                            {
                                                                .set_layouts = set_layouts,
                                                            })
                                    .value();

  std::vector<VkVertexInputBindingDescription> binding_descriptions = {{
      .binding = 0,
      .stride = sizeof(Vec3),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }}; // Vertex input

  std::vector<VkVertexInputAttributeDescription> attribute_descriptions = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0}};

  shadow_map_pipeline_ = renderer_.pipeline_manager().create_graphics_pipeline({
      .layout = shadow_map_pipeline_layout_,
      .pipeline_rendering_create_info =
          vkh::PipelineRenderingCreateInfo{
              .color_attachment_formats = {},
              .depth_attachment_format = shadow_map_format_,
          },
      .vertex_input_state_create_info = {.binding_descriptions = binding_descriptions,
                                         .attribute_descriptions = attribute_descriptions},
      .stages = {{vertex_shader}},
      .rasterization_state = {.depth_bias_info =
                                  DepthBiasInfo{.constant_factor = 1.25f, .slope_factor = 1.75f}},
      .debug_name = "Shadow Mapping Graphics Pipeline",
  });
}

void ShadowMapRenderer::record_commands(VkCommandBuffer cmd)
{
  ZoneScopedN("Shadow Render Pass");
  TracyVkZone(renderer_.current_frame().tracy_vk_ctx, cmd, "Shadow Render Pass");
  vkh::Context& context = renderer_.context();

  const usize draw_count = renderer_.draw_solid_objects().size();
  BEYOND_ENSURE(draw_count <= max_object_count);
  {
    ZoneScopedN("Copy Buffers");

    auto* object_transform_data =
        static_cast<Mat4*>(context.map(renderer_.current_frame().transform_buffer).value());
    auto* indirect_buffer_data = static_cast<VkDrawIndexedIndirectCommand*>(
        context.map(renderer_.current_frame().indirect_buffer).value());

    const auto draws = renderer_.draw_solid_objects();
    for (usize i = 0; i < draw_count; ++i) {
      const RenderObject& render_object = draws[i];
      object_transform_data[i] = renderer_.scene().global_transforms.at(render_object.node_index);
      indirect_buffer_data[i] = VkDrawIndexedIndirectCommand{
          .indexCount = render_object.submesh->index_count,
          .instanceCount = 1,
          .firstIndex = render_object.submesh->index_offset,
          .vertexOffset = narrow<i32>(render_object.submesh->vertex_offset),
          .firstInstance = narrow<u32>(i),
      };
    }
    // TODO: how to handle shadows for transparent objects?
    context.unmap(renderer_.current_frame().transform_buffer);
    context.unmap(renderer_.current_frame().indirect_buffer);
  }

  vkh::cmd_pipeline_barrier2(
      cmd, {.image_barriers = std::array{
                vkh::ImageBarrier2{.stage_masks = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                   VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
                                   .access_masks = {VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                                   .layouts = {VK_IMAGE_LAYOUT_UNDEFINED,
                                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
                                   .image = shadow_map_image_.image,
                                   .subresource_range =
                                       vkh::SubresourceRange{
                                           .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                       }}
                    .to_vk_struct() //
            }});

  const VkRenderingAttachmentInfo depth_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = shadow_map_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = VkClearValue{.depthStencil = {.depth = 1.f}},
  };

  const VkOffset2D offset = {0, 0};
  const VkExtent2D extent{.width = shadow_map_width_, .height = shadow_map_height_};
  const VkRenderingInfo render_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .offset = offset,
              .extent = extent,
          },
      .layerCount = 1,
      .pDepthAttachment = &depth_attachments_info,
  };

  vkCmdBeginRendering(cmd, &render_info);

  const VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = narrow<f32>(shadow_map_width_),
      .height = narrow<f32>(shadow_map_height_),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor{.offset = offset, .extent = extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Bind scene data
  const size_t frame_index = renderer_.current_frame_index();
  const u32 uniform_offset =
      narrow<u32>(context.align_uniform_buffer_size(sizeof(GPUSceneParameters))) *
      narrow<u32>(frame_index);

  renderer_.pipeline_manager().cmd_bind_pipeline(cmd, shadow_map_pipeline_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout_, 0, 1,
                          &renderer_.current_frame().global_descriptor_set, 1, &uniform_offset);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout_, 1, 1,
                          &renderer_.current_frame().object_descriptor_set, 0, nullptr);

  static constexpr VkDeviceSize vertex_offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &renderer_.scene_mesh_buffers.position_buffer.buffer,
                         &vertex_offset);
  vkCmdBindIndexBuffer(cmd, renderer_.scene_mesh_buffers.index_buffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  vkh::cmd_begin_debug_utils_label(cmd, "shadow mapping pass", {0.5, 0.5, 0.5, 1.0});
  vkCmdDrawIndexedIndirect(cmd, renderer_.current_frame().indirect_buffer, 0,
                           narrow<u32>(draw_count), sizeof(VkDrawIndexedIndirectCommand));
  vkh::cmd_end_debug_utils_label(cmd);

  vkCmdEndRendering(cmd);

  vkh::cmd_pipeline_barrier2(
      cmd, {.image_barriers = std::array{
                vkh::ImageBarrier2{.stage_masks = {VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
                                   .access_masks = {VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
                                   .layouts = {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                   .image = shadow_map_image_.image,
                                   .subresource_range =
                                       vkh::SubresourceRange{
                                           .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                       }}
                    .to_vk_struct() //
            }});
}

} // namespace charlie