#include "renderer.hpp"

#include "../vulkan_helpers/bda.hpp"
#include "../vulkan_helpers/debug_utils.hpp"
#include "../vulkan_helpers/error_handling.hpp"
#include "../vulkan_helpers/graphics_pipeline.hpp"
#include "../vulkan_helpers/initializers.hpp"
#include "../vulkan_helpers/pipeline_barrier.hpp"

#include "descriptor_allocator.hpp"

#include "pipeline_manager.hpp"

#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/utils/defer.hpp>

#include "camera.hpp"
#include "mesh.hpp"

#include "imgui_render_pass.hpp"

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace {

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
  beyond::Vec3 position;
};

void transit_current_swapchain_image_for_rendering(VkCommandBuffer cmd,
                                                   VkImage current_swapchain_image)
{
  ZoneScoped;

  const auto image_memory_barrier_to_render =
      vkh::ImageBarrier{
          .stage_masks = {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
          .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
          .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          .image = current_swapchain_image,
          .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
      }
          .to_vk_struct();
  vkh::cmd_pipeline_barrier(cmd, {.image_barriers = std::array{image_memory_barrier_to_render}});
}

void transit_current_swapchain_image_to_present(VkCommandBuffer cmd,
                                                VkImage current_swapchain_image)
{
  ZoneScoped;

  const auto image_memory_barrier_to_present =
      vkh::ImageBarrier{
          .stage_masks = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
          .access_masks = {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE},
          .layouts = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
          .image = current_swapchain_image,
          .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
      }
          .to_vk_struct();
  vkh::cmd_pipeline_barrier(cmd, {.image_barriers = std::array{image_memory_barrier_to_present}});
}

} // anonymous namespace

namespace charlie {

Renderer::Renderer(Window& window, InputHandler& input_handler)
    : window_{&window}, resolution_{window.resolution()}, context_{window},
      graphics_queue_{context_.graphics_queue()},
      sampler_cache_{std::make_unique<SamplerCache>(context_.device())},
      swapchain_{context_, {.extent = to_extent2d(resolution_)}},
      upload_context_{init_upload_context(context_).expect("Failed to create upload context")},
      frame_deletion_queue_{DeletionQueue{context_}, DeletionQueue{context_}},
      shader_compiler_{std::make_unique<ShaderCompiler>()},
      pipeline_manager_{std::make_unique<PipelineManager>(context_)},
      textures_{std::make_unique<TextureManager>(context_, upload_context_,
                                                 sampler_cache_->default_sampler())}
{
  init_depth_image();
  init_final_hdr_image();

  shadow_map_renderer_ = std::make_unique<ShadowMapRenderer>(*this, ref(*sampler_cache_));

  init_frame_data();
  init_descriptors();
  init_pipelines();

  shadow_map_renderer_->init_pipeline();

  {
    ZoneScopedN("Imgui Render Pass");

    imgui_render_pass_ =
        std::make_unique<ImguiRenderPass>(*this, window.raw_window(), swapchain_.image_format());
  }

  input_handler.add_listener(std::bind_front(&Renderer::on_input_event, std::ref(*this)));
}

auto Renderer::upload_image(const charlie::CPUImage& cpu_image, const ImageUploadInfo& upload_info)
    -> VkImage
{
  return textures_->upload_image(cpu_image, upload_info);
}

void Renderer::init_frame_data()
{
  ZoneScoped;
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index()};

  for (unsigned int i = 0; i < frame_overlap; ++i) {
    FrameData& frame = frames_[i];
    VK_CHECK(
        vkCreateCommandPool(context_, &command_pool_create_info, nullptr, &frame.command_pool));
    frame.main_command_buffer =
        vkh::allocate_command_buffer(context_,
                                     {
                                         .command_pool = frame.command_pool,
                                         .debug_name = fmt::format("Main Command Buffer {}", i),
                                     })
            .value();
    frame.present_semaphore =
        vkh::create_semaphore(context_, {.debug_name = fmt::format("Present Semaphore {}", i)})
            .value();
    frame.render_semaphore =
        vkh::create_semaphore(context_, {.debug_name = fmt::format("Render Semaphore {}", i)})
            .value();
    frame.render_fence =
        vkh::create_fence(context_, {.flags = VK_FENCE_CREATE_SIGNALED_BIT,
                                     .debug_name = fmt::format("Render Fence {}", i)})
            .value();

    frame.tracy_vk_ctx = TracyVkContext(context_.physical_device(), context_.device(),
                                        context_.graphics_queue(), frame.main_command_buffer);
  }
}

void Renderer::init_depth_image()
{
  ZoneScoped;

  depth_image_ =
      vkh::create_image(context_,
                        vkh::ImageCreateInfo{
                            .format = depth_format,
                            .extent = VkExtent3D{resolution_.width, resolution_.height, 1},
                            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            .debug_name = "Depth Image",
                        })
          .expect("Fail to create depth image");
  depth_image_view_ =
      vkh::create_image_view(
          context_, vkh::ImageViewCreateInfo{.image = depth_image_.image,
                                             .format = depth_format,
                                             .subresource_range =
                                                 vkh::SubresourceRange{
                                                     .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 },
                                             .debug_name = "Depth Image View"})
          .expect("Fail to create depth image view");
}

void Renderer::init_final_hdr_image()
{
  ZoneScoped;

  final_hdr_image_ =
      vkh::create_image(
          context_,
          vkh::ImageCreateInfo{
              .format = final_hdr_image_format,
              .extent = VkExtent3D{resolution_.width, resolution_.height, 1},
              .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              .debug_name = "Final HDR Image",
          })
          .expect("Fail to create final hdr image");
  final_hdr_image_view_ =
      vkh::create_image_view(
          context_, vkh::ImageViewCreateInfo{.image = final_hdr_image_.image,
                                             .format = final_hdr_image_format,
                                             .subresource_range =
                                                 vkh::SubresourceRange{
                                                     .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                 },
                                             .debug_name = "Final HDR Image View"})
          .expect("Fail to create final hdr image view");
}

void Renderer::init_descriptors()
{
  ZoneScoped;

  descriptor_allocator_ = std::make_unique<DescriptorAllocator>(context_);
  descriptor_layout_cache_ = std::make_unique<DescriptorLayoutCache>(context_.device());

  static constexpr VkDescriptorSetLayoutBinding material_bindings[] = {
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  material_descriptor_set_layout = descriptor_layout_cache_
                                       ->create_descriptor_set_layout({
                                           .p_next = nullptr,
                                           .bindings = material_bindings,
                                       })
                                       .value();

  const size_t scene_param_buffer_size =
      frame_overlap * context_.align_uniform_buffer_size(sizeof(GPUSceneParameters));
  scene_parameter_buffer_ = vkh::create_buffer(context_,
                                               {
                                                   .size = scene_param_buffer_size,
                                                   .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                   .debug_name = "Scene Parameter buffer",
                                               })
                                .value();

  for (auto i = 0u; i < frame_overlap; ++i) {
    FrameData& frame = frames_[i];
    frame.camera_buffer = vkh::create_buffer(context_,
                                             vkh::BufferCreateInfo{
                                                 .size = sizeof(GPUCameraData),
                                                 .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                 .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                 .debug_name = fmt::format("Camera Buffer {}", i),
                                             })
                              .value();

    frame.transform_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(Mat4) * max_object_count,
                               .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Objects Buffer {}", i),
                           })
            .value();

    // Global
    const VkDescriptorBufferInfo camera_buffer_info = {
        .buffer = frame.camera_buffer.buffer, .offset = 0, .range = sizeof(GPUCameraData)};
    const VkDescriptorBufferInfo scene_buffer_info = {
        .buffer = scene_parameter_buffer_, .offset = 0, .range = sizeof(GPUSceneParameters)};

    auto global_descriptor_build_result =
        DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
            .bind_buffer(0, camera_buffer_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(1, scene_buffer_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(2, shadow_map_renderer_->shadow_map_image_info(),
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build()
            .value();
    if (global_descriptor_set_layout == VK_NULL_HANDLE) {
      global_descriptor_set_layout = global_descriptor_build_result.layout;
    } else {
      BEYOND_ENSURE(global_descriptor_set_layout == global_descriptor_build_result.layout);
    }
    frame.global_descriptor_set = global_descriptor_build_result.set;

    // Objects
    const VkDescriptorBufferInfo transform_buffer_info = {.buffer = frame.transform_buffer.buffer,
                                                          .offset = 0,
                                                          .range = sizeof(Mat4) * max_object_count};

    auto objects_descriptor_build_result =
        DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
            .bind_buffer(0, transform_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT)
            .build()
            .value();
    object_descriptor_set_layout = objects_descriptor_build_result.layout;
    frame.object_descriptor_set = objects_descriptor_build_result.set;
  }
}

void Renderer::init_pipelines()
{
  ZoneScoped;

  init_generate_draws_pipeline();

  init_mesh_pipeline();

  init_tonemapping_pipeline();
}

struct GenerateDrawsPushConstant {
  VkDeviceAddress draws_buffer_address = 0;
  VkDeviceAddress draws_indirect_buffer_address = 0;
  u32 total_draws_count = 0;
};

void Renderer::init_generate_draws_pipeline()
{
  ZoneScoped;

  const ShaderHandle shader =
      pipeline_manager_->add_shader("generate_draws.comp.glsl", ShaderStage::compute);

  const VkPushConstantRange push_constant[] = {{
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .offset = 0,
      .size = sizeof(GenerateDrawsPushConstant),
  }};

  generate_draws_layout_ = vkh::create_pipeline_layout(context_,
                                                       {
                                                           .push_constant_ranges = push_constant,
                                                       })
                               .value();

  const auto create_info = ComputePipelineCreateInfo{.layout = generate_draws_layout_,
                                                     .stage =
                                                         {
                                                             .handle = shader,
                                                         },
                                                     .debug_name = "Generate Draws Pipeline"};
  generate_draws_pipeline_ = pipeline_manager_->create_compute_pipeline(create_info);
}

void Renderer::init_mesh_pipeline()
{
  ZoneScoped;

  const ShaderHandle vertex_shader =
      pipeline_manager_->add_shader("mesh.vert.glsl", ShaderStage::vertex);
  const ShaderHandle fragment_shader =
      pipeline_manager_->add_shader("mesh.frag.glsl", ShaderStage::fragment);

  {
    ZoneScopedN("Create Pipeline Layout");
    const VkDescriptorSetLayout set_layouts[] = {
        global_descriptor_set_layout, object_descriptor_set_layout, material_descriptor_set_layout,
        textures_->descriptor_set_layout()};

    const VkPushConstantRange push_constant[] = {{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(MeshPushConstant),
    }};

    mesh_pipeline_layout_ = vkh::create_pipeline_layout(context_,
                                                        {
                                                            .set_layouts = set_layouts,
                                                            .push_constant_ranges = push_constant,
                                                        })
                                .value();
  }

  auto create_info = charlie::GraphicsPipelineCreateInfo{
      .layout = mesh_pipeline_layout_,
      .pipeline_rendering_create_info =
          vkh::PipelineRenderingCreateInfo{
              .color_attachment_formats = {final_hdr_image_format},
              .depth_attachment_format = depth_format,
          },
      .stages = {{vertex_shader}, {fragment_shader}},
      .rasterization_state = {.cull_mode = VK_CULL_MODE_BACK_BIT},
      .depth_stencil_state =
          {
              .depth_test_enable = VK_TRUE,
              .depth_write_enable = VK_TRUE,
              .depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL,
          },
      .debug_name = "Mesh Graphics Pipeline",
  };

  mesh_pipeline_ = pipeline_manager_->create_graphics_pipeline(create_info);

  create_info.color_blending = vkh::color_blend_attachment_additive();
  create_info.debug_name = "Mesh Graphics Pipeline (Transparent)";
  mesh_pipeline_transparent_ = pipeline_manager_->create_graphics_pipeline(create_info);
}

void Renderer::init_tonemapping_pipeline()
{
  ZoneScoped;

  const ShaderHandle vertex_shader =
      pipeline_manager_->add_shader("fullscreen_tri.vert.glsl", ShaderStage::vertex);
  const ShaderHandle fragment_shader =
      pipeline_manager_->add_shader("tonemapping.frag.glsl", ShaderStage::fragment);

  const VkDescriptorImageInfo image_info{
      .sampler = sampler_cache_->default_blocky_sampler(),
      .imageView = final_hdr_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const auto result = DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
                          .bind_image(0, image_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                          .build()
                          .value();

  final_hdr_image_descriptor_set_ = result.set;

  const VkDescriptorSetLayout set_layouts[] = {result.layout};

  tonemapping_pipeline_layout_ = vkh::create_pipeline_layout(context_,
                                                             {
                                                                 .set_layouts = set_layouts,
                                                             })
                                     .value();

  auto create_info = charlie::GraphicsPipelineCreateInfo{
      .layout = tonemapping_pipeline_layout_,
      .pipeline_rendering_create_info =
          vkh::PipelineRenderingCreateInfo{
              .color_attachment_formats = {swapchain_.image_format()},
          },
      .stages = {{vertex_shader}, {fragment_shader}},
      .rasterization_state = {.cull_mode = VK_CULL_MODE_BACK_BIT},
      .debug_name = "Tone Mapping Pipeline",
  };
  tonemapping_pipeline_ = pipeline_manager_->create_graphics_pipeline(create_info);
}

void Renderer::update(const charlie::Camera& camera)
{
  ZoneScopedN("Update");

  pipeline_manager_->update();

  textures_->update();

  imgui_render_pass_->pre_render();

  {
    // Camera
    const Mat4 view = camera.view_matrix();
    const Mat4 projection = camera.proj_matrix();

    const GPUCameraData cam_data = {
        .view = view,
        .proj = projection,
        .view_proj = projection * view,
        .position = camera.position(),
    };

    const vkh::AllocatedBuffer& camera_buffer = current_frame().camera_buffer;

    void* data = nullptr;
    vmaMapMemory(context_.allocator(), camera_buffer.allocation, &data);
    memcpy(data, &cam_data, sizeof(GPUCameraData));
    vmaUnmapMemory(context_.allocator(), camera_buffer.allocation);

    // Scene data
    const auto dir = Vec3(scene_parameters_.sunlight_direction.xyz);
    const Vec3 up =
        abs(dot(dir, Vec3(0.0, 1.0, 0.0))) > 0.9 ? Vec3(0.0, 0.0, 1.0) : Vec3(0.0, 1.0, 0.0);

    // TODO: move this to ShadowMapRenderer
    scene_parameters_.sunlight_view_proj = beyond::ortho(-20.f, 20.f, 20.f, -20.f, -100.f, 100.f) *
                                           beyond::look_at(-dir, Vec3(0.0), up);

    char* scene_data = static_cast<char*>(context_.map(scene_parameter_buffer_).value());
    const size_t frame_index = frame_number_ % frame_overlap;

    scene_data += context_.align_uniform_buffer_size(sizeof(GPUSceneParameters)) * frame_index;
    memcpy(scene_data, &scene_parameters_, sizeof(GPUSceneParameters));
    context_.unmap(scene_parameter_buffer_);
  }
}

void Renderer::render(const charlie::Camera& camera)
{
  ZoneScopedN("Render");

  update(camera);

  const auto& frame = current_frame();
  constexpr u64 one_second = 1'000'000'000;

  // wait until the GPU has finished rendering the last frame.
  {
    ZoneScopedN("wait for render fence");
    VK_CHECK(vkWaitForFences(context_, 1, &frame.render_fence, true, one_second));
  }

  {
    const VkResult result = swapchain_.acquire_next_image(frame.present_semaphore);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return; }
    VK_CHECK(result);
  }

  VK_CHECK(vkResetFences(context_, 1, &frame.render_fence));
  VK_CHECK(vkResetCommandBuffer(frame.main_command_buffer, 0));

  {
    ZoneScopedN("Fill Transform Buffers");
    auto* object_transform_data =
        static_cast<Mat4*>(context_.map(current_frame().transform_buffer).value());
    for (usize i = 0; i < scene_->global_transforms.size(); ++i) {
      object_transform_data[i] = scene_->global_transforms[i];
    }
    context_.unmap(current_frame().transform_buffer);
  }

  current_frame_deletion_queue().flush();

  VkCommandBuffer cmd = frame.main_command_buffer;

  const auto current_swapchain_image = swapchain_.current_image();
  const auto current_swapchain_image_view = swapchain_.current_image_view();

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  {
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    TracyVkCollect(frame.tracy_vk_ctx, cmd);

    {
      ZoneScopedN("Generate Draws");
      TracyVkZone(frame.tracy_vk_ctx, cmd, "Generate Draws");

      vkh::cmd_begin_debug_utils_label(cmd, "Generate Draws Pass", {0.097f, 0.01f, 0.049f, 1.0f});

      const GenerateDrawsPushConstant push_constant{
          .draws_buffer_address = vkh::get_buffer_device_address(context_, draws_buffer_),
          .draws_indirect_buffer_address =
              vkh::get_buffer_device_address(context_, draws_indirect_buffer_),
          .total_draws_count = total_draw_count_,
      };
      vkCmdPushConstants(cmd, generate_draws_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(GenerateDrawsPushConstant), &push_constant);

      pipeline_manager_->cmd_bind_pipeline(cmd, generate_draws_pipeline_);

      static constexpr u32 local_group_size = 256;
      vkCmdDispatch(cmd, total_draw_count_ / local_group_size + 1, 1, 1);

      vkh::cmd_end_debug_utils_label(cmd);

      {
        const auto barrier =
            vkh::BufferMemoryBarrier{
                .stage_masks = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT},
                .access_masks = {VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT},
                .buffer = draws_indirect_buffer_.buffer,
                .offset = 0,
                .size = total_draw_count_ * sizeof(VkDrawIndexedIndirectCommand),
            }
                .to_vk_struct();

        vkh::cmd_pipeline_barrier(cmd, {.buffer_memory_barriers = std::array{barrier}});
      }
    }

    {
      TracyVkZone(frame.tracy_vk_ctx, cmd, "Swapchain");

      transit_current_swapchain_image_for_rendering(cmd, current_swapchain_image);

      if (scene_parameters_.sunlight_shadow_mode != 0) {
        shadow_map_renderer_->record_commands(cmd);
      }

      {
        // Prepare final HDR image
        const auto image_memory_barrier_to_render =
            vkh::ImageBarrier{
                .stage_masks = {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
                .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                .image = final_hdr_image_.image,
                .subresource_range =
                    vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
            }
                .to_vk_struct();
        vkh::cmd_pipeline_barrier(cmd,
                                  {.image_barriers = std::array{image_memory_barrier_to_render}});
      }
      draw_scene(cmd);
      {
        // Transit final hdr image to sample
        const auto image_memory_barrier_to_sample =
            vkh::ImageBarrier{
                .stage_masks = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
                .access_masks = {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
                .layouts = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                .image = final_hdr_image_.image,
                .subresource_range =
                    vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
            }
                .to_vk_struct();
        vkh::cmd_pipeline_barrier(cmd,
                                  {.image_barriers = std::array{image_memory_barrier_to_sample}});
      }

      {
        // Tonemapping pipeline
        const VkRenderingAttachmentInfo color_attachments_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = current_swapchain_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        const VkRenderingInfo render_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = to_extent2d(resolution()),
                },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachments_info,
        };

        vkCmdBeginRendering(cmd, &render_info);

        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = beyond::narrow<float>(resolution().width),
            .height = beyond::narrow<float>(resolution().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        const VkRect2D scissor{.offset = {0, 0}, .extent = to_extent2d(resolution())};
        vkCmdSetScissor(cmd, 0, 1, &scissor);
      }

      pipeline_manager_->cmd_bind_pipeline(cmd, tonemapping_pipeline_);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapping_pipeline_layout_, 0,
                              1, &final_hdr_image_descriptor_set_, 0, nullptr);
      // Draw a fullscreen triangle

      vkh::cmd_begin_debug_utils_label(cmd, "tonemapping pass", {0.1f, 0.1f, 0.1f, 1.0f});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkh::cmd_end_debug_utils_label(cmd);

      vkCmdEndRendering(cmd);

      {
        ZoneScopedN("ImGUI Render Pass");
        TracyVkZone(frame.tracy_vk_ctx, cmd, "ImGUI Render Pass");
        imgui_render_pass_->render(cmd, current_swapchain_image_view);
      }

      transit_current_swapchain_image_to_present(cmd, current_swapchain_image);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));
  }

  {
    ZoneScopedN("vkQueueSubmit");
    static constexpr VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.present_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame.render_semaphore,
    };

    VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit, frame.render_fence));
  }

  present();

  ++frame_number_;
}

void Renderer::present()
{
  ZoneScopedN("vkQueuePresentKHR");

  const auto& frame = current_frame();

  VkSwapchainKHR swapchain = swapchain_.get();
  const std::uint32_t image_index = swapchain_.current_image_index();

  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame.render_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &image_index,
  };

  const VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // Supress
    return;
  }
  VK_CHECK(result);
}

[[nodiscard]] auto Renderer::upload_mesh_buffer(const CPUMeshBuffers& buffers,
                                                std::string_view name) -> MeshBuffers
{
  ZoneScoped;

  static constexpr auto vertex_buffer_usage =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  vkh::AllocatedBuffer position_buffer =
      upload_buffer(context_, upload_context_, buffers.positions,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | vertex_buffer_usage,
                    fmt::format("{} Vertex Position", name))
          .value();
  vkh::AllocatedBuffer vertex_buffer =
      upload_buffer(context_, upload_context_, buffers.vertices, vertex_buffer_usage,
                    fmt::format("{} Vertex", name))
          .value();
  vkh::AllocatedBuffer index_buffer =
      upload_buffer(context_, upload_context_, buffers.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    fmt::format("{} Index", name))
          .value();

  return MeshBuffers{
      .position_buffer = position_buffer,
      .vertex_buffer = vertex_buffer,
      .index_buffer = index_buffer,
  };
}

auto Renderer::add_mesh(const CPUMesh& cpu_mesh) -> MeshHandle
{
  std::vector<SubMesh> submeshes;
  submeshes.reserve(cpu_mesh.submeshes.size());
  for (const auto& submesh : cpu_mesh.submeshes) {
    submeshes.push_back(SubMesh{.vertex_offset = submesh.vertex_offset,
                                .index_offset = submesh.index_offset,
                                .index_count = submesh.index_count,
                                .material_index = submesh.material_index.value()});
  }

  return meshes_.insert(Mesh{
      .submeshes = std::move(submeshes),
      .aabb = cpu_mesh.aabb,
  });
}

void Renderer::draw_scene(VkCommandBuffer cmd)
{
  ZoneScopedN("Mesh Render Pass");
  TracyVkZone(current_frame().tracy_vk_ctx, cmd, "Mesh Render Pass");

  const VkRenderingAttachmentInfo color_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = final_hdr_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = VkClearValue{.color = {.float32 = {1.0f, 1.0f, 1.0f, 1.0f}}},
  };

  const VkRenderingAttachmentInfo depth_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = depth_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = VkClearValue{.depthStencil = {.depth = 0.f}},
  };

  const VkRenderingInfo render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = to_extent2d(resolution()),
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachments_info,
      .pDepthAttachment = &depth_attachments_info,
  };

  vkCmdBeginRendering(cmd, &render_info);

  const VkViewport viewport{
      .x = 0.0f,
      .y = beyond::narrow<float>(resolution().height),
      .width = beyond::narrow<float>(resolution().width),
      .height = -beyond::narrow<float>(resolution().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor{.offset = {0, 0}, .extent = to_extent2d(resolution())};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  const size_t frame_index = frame_number_ % frame_overlap;

  // bind descriptor sets
  const u32 uniform_offset =
      beyond::narrow<u32>(context_.align_uniform_buffer_size(sizeof(GPUSceneParameters))) *
      beyond::narrow<u32>(frame_index);

  VkDescriptorSet texture_descriptor_set = textures_->descriptor_set();
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 0, 1,
                          &current_frame().global_descriptor_set, 1, &uniform_offset);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 1, 1,
                          &current_frame().object_descriptor_set, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 2, 1,
                          &material_descriptor_set_, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 3, 1,
                          &texture_descriptor_set, 0, nullptr);

  // Vertex and index buffers
  vkCmdBindIndexBuffer(cmd, scene_mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

  const MeshPushConstant push_constant{
      .position_buffer_address =
          vkh::get_buffer_device_address(context_, scene_mesh_buffers.position_buffer),
      .vertex_buffer_address =
          vkh::get_buffer_device_address(context_, scene_mesh_buffers.vertex_buffer),
      .draws_buffer_address = vkh::get_buffer_device_address(context_, draws_buffer_),
  };
  vkCmdPushConstants(cmd, mesh_pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(MeshPushConstant), &push_constant);

  // draw solid objects
  {
    pipeline_manager_->cmd_bind_pipeline(cmd, mesh_pipeline_);
    vkh::cmd_begin_debug_utils_label(cmd, "solid objects pass", {0.084f, 0.135f, 0.394f, 1.0f});
    vkCmdDrawIndexedIndirect(cmd, draws_indirect_buffer_, 0, solid_draw_count_,
                             sizeof(VkDrawIndexedIndirectCommand));
    vkh::cmd_end_debug_utils_label(cmd);
  }

  // Draw transparent objects
  if (transparent_draw_count_ > 0) {
    {
      const auto draw_buffer_offset = solid_draw_count_ * sizeof(VkDrawIndexedIndirectCommand);
      pipeline_manager_->cmd_bind_pipeline(cmd, mesh_pipeline_transparent_);
      vkh::cmd_begin_debug_utils_label(cmd, "transparent objects pass", {1.0f, 0.9f, 0.9f, 1.0f});
      vkCmdDrawIndexedIndirect(cmd, draws_indirect_buffer_, draw_buffer_offset,
                               transparent_draw_count_, sizeof(VkDrawIndexedIndirectCommand));
      vkh::cmd_end_debug_utils_label(cmd);
    }
  }

  vkCmdEndRendering(cmd);
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  imgui_render_pass_ = nullptr;

  vkh::destroy_buffer(context_, scene_mesh_buffers.vertex_buffer);
  vkh::destroy_buffer(context_, scene_mesh_buffers.position_buffer);
  vkh::destroy_buffer(context_, scene_mesh_buffers.index_buffer);
  scene_ = nullptr;

  textures_ = nullptr;

  vkDestroyCommandPool(context_, upload_context_.command_pool, nullptr);
  vkDestroyFence(context_, upload_context_.fence, nullptr);

  vkDestroyPipelineLayout(context_, tonemapping_pipeline_layout_, nullptr);
  vkDestroyPipelineLayout(context_, mesh_pipeline_layout_, nullptr);

  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);

  shadow_map_renderer_ = nullptr;

  descriptor_allocator_ = nullptr;
  descriptor_layout_cache_ = nullptr;

  vkh::destroy_buffer(context_, scene_parameter_buffer_);
  for (auto& frame : frames_) {
    TracyVkDestroy(frame.tracy_vk_ctx);

    vkh::destroy_buffer(context_, frame.camera_buffer);

    vkDestroyFence(context_, frame.render_fence, nullptr);
    vkDestroySemaphore(context_, frame.render_semaphore, nullptr);
    vkDestroySemaphore(context_, frame.present_semaphore, nullptr);
    vkDestroyCommandPool(context_, frame.command_pool, nullptr);
  }

  sampler_cache_ = nullptr;
}

void Renderer::on_input_event(const Event& event, const InputStates& /*states*/)
{
  std::visit(
      [this](auto e) {
        if constexpr (std::is_same_v<decltype(e), WindowEvent>) {
          if (e.window_id == window_->window_id() && e.type == WindowEventType::resize) {
            this->resize();
          }
        }
      },
      event);
}

void Renderer::resize()
{
  context_.wait_idle();

  resolution_ = window_->resolution();

  // recreate swapchain
  swapchain_ =
      vkh::Swapchain(context_, vkh::SwapchainCreateInfo{.extent = charlie::to_extent2d(resolution_),
                                                        .old_swapchain = swapchain_.get()});

  // recreate depth image
  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);
  init_depth_image();

  vkDestroyImageView(context_, final_hdr_image_view_, nullptr);
  vkh::destroy_image(context_, final_hdr_image_);
  init_final_hdr_image();

  const VkDescriptorImageInfo image_info{
      .sampler = sampler_cache_->default_blocky_sampler(),
      .imageView = final_hdr_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const auto result = DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
                          .bind_image(0, image_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                          .build()
                          .value();

  final_hdr_image_descriptor_set_ = result.set;
}

auto Renderer::add_material(const CPUMaterial& material_info) -> u32
{
  const u32 default_white_index = textures_->default_white_texture_index();
  const u32 default_normal_index = textures_->default_normal_texture_index();

  const u32 albedo_texture_index = material_info.albedo_texture_index.value_or(default_white_index);
  const u32 normal_texture_index =
      material_info.normal_texture_index.value_or(default_normal_index);
  const u32 metallic_roughness_texture_index =
      material_info.metallic_roughness_texture_index.value_or(default_white_index);
  const u32 occlusion_texture_index =
      material_info.occlusion_texture_index.value_or(default_white_index);
  const u32 emissive_texture_index =
      material_info.emissive_texture_index.value_or(default_white_index);

  materials_.push_back(Material{
      .base_color_factor = material_info.base_color_factor,
      .albedo_texture_index = albedo_texture_index,
      .normal_texture_index = normal_texture_index,
      .metallic_roughness_texture_index = metallic_roughness_texture_index,
      .occlusion_texture_index = occlusion_texture_index,
      .emissive_factor = material_info.emissive_factor,
      .emissive_texture_index = emissive_texture_index,
      .metallic_factor = material_info.metallic_factor,
      .roughness_factor = material_info.roughness_factor,
      .alpha_cutoff =
          material_info.alpha_mode == AlphaMode::mask ? material_info.alpha_cutoff : 0.0f,
  });

  material_alpha_modes_.push_back(material_info.alpha_mode);
  BEYOND_ASSERT(materials_.size() == material_alpha_modes_.size());

  return narrow<u32>(materials_.size() - 1);
}

void Renderer::upload_materials()
{
  material_buffer_ = upload_buffer(context_, upload_context_, materials_,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Materials")
                         .value();

  const VkDescriptorBufferInfo material_buffer_info = {
      .buffer = material_buffer_,
      .offset = 0,
      .range = materials_.size() * sizeof(Material),
  };

  auto result = DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
                    .bind_buffer(0, material_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build()
                    .value();
  BEYOND_ENSURE(material_descriptor_set_layout == result.layout);
  material_descriptor_set_ = result.set;
}

auto Renderer::add_texture(Texture texture) -> u32
{
  return textures_->add_texture(texture);
}

void Renderer::set_scene(std::unique_ptr<Scene> scene)
{
  BEYOND_ENSURE(scene != nullptr);
  scene_ = std::move(scene);

  populate_scene_draw_buffers();
}

void Renderer::populate_scene_draw_buffers()
{
  ZoneScoped;
  std::vector<Draw> draws;

  for (const auto [node_index, render_component] : scene_->render_components) {
    const Mesh& mesh = meshes_.try_get(render_component.mesh).expect("Cannot find mesh by handle!");

    for (const auto& submesh : mesh.submeshes) {
      draws.push_back(Draw{
          .vertex_offset = submesh.vertex_offset,
          .index_count = submesh.index_count,
          .index_offset = submesh.index_offset,
          .material_index = submesh.material_index,
          .node_index = node_index,
      });
    }
  }

  // Partition Transparent objects
  auto pivot = std::partition(draws.begin(), draws.end(), [&](Draw draw) {
    const AlphaMode alpha_mode = material_alpha_modes_.at(draw.material_index);
    return alpha_mode != AlphaMode::blend;
  });
  total_draw_count_ = narrow<u32>(draws.size());
  solid_draw_count_ = narrow<u32>(pivot - draws.begin());
  transparent_draw_count_ = narrow<u32>(draws.end() - pivot);

  current_frame_deletion_queue().push(
      [draws_buffer = draws_buffer_,
       draw_indirect_buffer = draws_indirect_buffer_](vkh::Context& context) {
        vkh::destroy_buffer(context, draws_buffer);
        vkh::destroy_buffer(context, draw_indirect_buffer);
      });

  draws_buffer_ =
      upload_buffer(context_, upload_context_, draws,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    "Draws")
          .value();

  draws_indirect_buffer_ =
      vkh::create_buffer(
          context_,
          vkh::BufferCreateInfo{.size = draws.size() * sizeof(VkDrawIndexedIndirectCommand),
                                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                .debug_name = "Draw Indirect"})
          .value();
}

} // namespace charlie
