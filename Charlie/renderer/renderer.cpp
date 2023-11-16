#include "renderer.hpp"

#include "../vulkan_helpers/commands.hpp"
#include "../vulkan_helpers/descriptor_pool.hpp"
#include "../vulkan_helpers/graphics_pipeline.hpp"
#include "../vulkan_helpers/image_view.hpp"
#include "../vulkan_helpers/shader_module.hpp"
#include "../vulkan_helpers/sync.hpp"
#include "descriptor_allocator.hpp"

#include "../shader_compiler/shader_compiler.hpp"
#include "pipeline_manager.hpp"

#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/utils/defer.hpp>

#include "camera.hpp"
#include "mesh.hpp"

#include "../utils/configuration.hpp"
#include "imgui_render_pass.hpp"

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace {

struct GPUObjectData {
  Mat4 model;
};

struct GPUCameraData {
  Mat4 view;
  Mat4 proj;
  Mat4 view_proj;
};

constexpr usize max_object_count = 10000;

void transit_current_swapchain_image_for_rendering(VkCommandBuffer cmd,
                                                   VkImage current_swapchain_image)
{
  ZoneScoped;

  const auto image_memory_barrier_to_render =
      vkh::ImageBarrier2{
          .stage_masks = {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
          .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
          .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          .image = current_swapchain_image,
          .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
      }
          .to_vk_struct();
  vkh::cmd_pipeline_barrier2(cmd, {.image_barriers = std::array{image_memory_barrier_to_render}});
}

void transit_current_swapchain_image_to_present(VkCommandBuffer cmd,
                                                VkImage current_swapchain_image)
{
  ZoneScoped;

  const auto image_memory_barrier_to_present =
      vkh::ImageBarrier2{
          .stage_masks = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
          .access_masks = {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE},
          .layouts = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
          .image = current_swapchain_image,
          .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
      }
          .to_vk_struct();
  vkh::cmd_pipeline_barrier2(cmd, {.image_barriers = std::array{image_memory_barrier_to_present}});
}

struct IndirectBatch {
  charlie::MeshHandle mesh;
  charlie::MaterialHandle material;
  u32 first = 0;
  u32 count = 0;
};

[[nodiscard]] auto compact_draws(std::span<const charlie::RenderObject> objects)
    -> std::vector<IndirectBatch>
{
  std::vector<IndirectBatch> draws;

  if (objects.empty()) return draws;

  beyond::optional<charlie::MeshHandle> last_mesh = beyond::nullopt;
  for (u32 i = 0; i < objects.size(); ++i) {
    const auto& object = objects[i];
    const bool same_mesh = object.mesh == last_mesh;
    if (same_mesh) {
      BEYOND_ASSERT(!draws.empty());
      ++draws.back().count;
    } else {
      draws.push_back(IndirectBatch{
          .mesh = object.mesh,
          .material = object.material,
          .first = i,
          .count = 1,
      });
      last_mesh = object.mesh;
    }
  }
  return draws;
}

} // anonymous namespace

namespace charlie {

Renderer::Renderer(Window& window, InputHandler& input_handler)
    : window_{&window}, resolution_{window.resolution()}, context_{window},
      graphics_queue_{context_.graphics_queue()},
      swapchain_{context_, {.extent = to_extent2d(resolution_)}}
{
  init_depth_image();
  init_shadow_map();
  init_frame_data();
  init_descriptors();

  shader_compiler_ = std::make_unique<ShaderCompiler>();
  pipeline_manager_ = std::make_unique<PipelineManager>(context_);

  init_pipelines();

  upload_context_ = init_upload_context(context_).expect("Failed to create upload context");

  imgui_render_pass_ =
      std::make_unique<ImguiRenderPass>(*this, window.raw_window(), swapchain_.image_format());

  init_sampler();
  init_default_texture();

  input_handler.add_listener(std::bind_front(&Renderer::on_input_event, std::ref(*this)));
}

auto Renderer::upload_image(const charlie::CPUImage& cpu_image, const ImageUploadInfo& upload_info)
    -> VkImage
{
  ZoneScoped;

  vkh::Context& context = context_;
  charlie::UploadContext& upload_context = upload_context_;

  BEYOND_ENSURE(cpu_image.width != 0 && cpu_image.height != 0);

  const void* pixel_ptr = static_cast<const void*>(cpu_image.data.get());
  const auto image_size = beyond::narrow<VkDeviceSize>(cpu_image.width) * cpu_image.height * 4;

  const auto staging_buffer_debug_name = cpu_image.name.empty()
                                             ? fmt::format("{} Staging Buffer", cpu_image.name)
                                             : "Image Staging Buffer";

  // allocate temporary buffer for holding texture data to upload
  auto staging_buffer =
      vkh::create_buffer(context, vkh::BufferCreateInfo{.size = image_size,
                                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
                                                        .debug_name = staging_buffer_debug_name})
          .value();
  BEYOND_DEFER(vkh::destroy_buffer(context, staging_buffer));

  // copy data to buffer
  void* data = context.map(staging_buffer).value();
  memcpy(data, pixel_ptr, beyond::narrow<size_t>(image_size));
  context.unmap(staging_buffer);

  const VkExtent3D image_extent = {
      .width = beyond::narrow<uint32_t>(cpu_image.width),
      .height = beyond::narrow<uint32_t>(cpu_image.height),
      .depth = 1,
  };

  const auto image_debug_name =
      cpu_image.name.empty() ? fmt::format("{} Image", cpu_image.name) : "Image";
  vkh::AllocatedImage allocated_image =
      vkh::create_image(context,
                        vkh::ImageCreateInfo{
                            .format = upload_info.format,
                            .extent = image_extent,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            .debug_name = image_debug_name,
                        })
          .expect("Failed to create image");

  immediate_submit(context, upload_context, [&](VkCommandBuffer cmd) {
    static constexpr VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    // barrier the image into the transfer-receive layout
    vkh::cmd_pipeline_barrier2(
        cmd, {.image_barriers = std::array{
                  vkh::ImageBarrier2{
                      .stage_masks = {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT},
                      .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                      .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                      .image = allocated_image.image,
                      .subresource_range = range}
                      .to_vk_struct() //
              }});

    const VkBufferImageCopy copy_region = {.bufferOffset = 0,
                                           .bufferRowLength = 0,
                                           .bufferImageHeight = 0,
                                           .imageSubresource =
                                               {
                                                   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                   .mipLevel = 0,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1,
                                               },
                                           .imageExtent = image_extent};

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, allocated_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    // barrier the image into the shader readable layout
    vkh::cmd_pipeline_barrier2(
        cmd, {.image_barriers = std::array{
                  vkh::ImageBarrier2{
                      .stage_masks = {VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
                      .access_masks = {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT},
                      .layouts = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                      .image = allocated_image.image,
                      .subresource_range = range}
                      .to_vk_struct() //
              }});
  });

  return images_.emplace_back(allocated_image).image;
}

void Renderer::init_shadow_map()
{
  ZoneScoped;

  shadow_map_image_ =
      vkh::create_image(
          context_,
          vkh::ImageCreateInfo{
              .format = depth_format_,
              .extent = VkExtent3D{shadow_map_width_, shadow_map_height_, 1},
              .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              .debug_name = "Shadow Map Image",
          })
          .expect("Fail to create shadow map image");
  shadow_map_image_view_ =
      vkh::create_image_view(
          context_, vkh::ImageViewCreateInfo{.image = shadow_map_image_.image,
                                             .format = depth_format_,
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
  vkCreateSampler(context_, &sampler_info, nullptr, &shadow_map_sampler_);
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
                            .format = depth_format_,
                            .extent = VkExtent3D{resolution_.width, resolution_.height, 1},
                            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            .debug_name = "Depth Image",
                        })
          .expect("Fail to create depth image");
  depth_image_view_ =
      vkh::create_image_view(
          context_, vkh::ImageViewCreateInfo{.image = depth_image_.image,
                                             .format = depth_format_,
                                             .subresource_range =
                                                 vkh::SubresourceRange{
                                                     .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 },
                                             .debug_name = "Depth Image View"})
          .expect("Fail to create depth image view");
}

void Renderer::init_descriptors()
{
  ZoneScoped;

  descriptor_allocator_ = std::make_unique<DescriptorAllocator>(context_);
  descriptor_layout_cache_ = std::make_unique<DescriptorLayoutCache>(context_.device());

  static constexpr VkDescriptorSetLayoutBinding cam_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

  static constexpr VkDescriptorSetLayoutBinding scene_buffer_binding = {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};

  static constexpr VkDescriptorSetLayoutBinding mesh_shadow_map_binding = {
      .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  static constexpr VkDescriptorSetLayoutBinding global_bindings[] = {
      cam_buffer_binding, scene_buffer_binding, mesh_shadow_map_binding};

  const vkh::DescriptorSetLayoutCreateInfo global_layout_create_info = {.bindings =
                                                                            global_bindings};

  global_descriptor_set_layout_ =
      descriptor_layout_cache_->create_descriptor_set_layout(global_layout_create_info).value();

  static constexpr VkDescriptorSetLayoutBinding object_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
  static constexpr VkDescriptorSetLayoutBinding object_bindings[] = {object_buffer_binding};

  object_descriptor_set_layout_ =
      descriptor_layout_cache_->create_descriptor_set_layout({.bindings = object_bindings}).value();

  static constexpr VkDescriptorSetLayoutBinding material_bindings[] = {
      // albedo
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      // normal
      {.binding = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      // occlusion
      {.binding = 2,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

  material_set_layout_ =
      descriptor_layout_cache_->create_descriptor_set_layout({.bindings = material_bindings})
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

    frame.object_buffer = vkh::create_buffer(context_,
                                             vkh::BufferCreateInfo{
                                                 .size = sizeof(GPUObjectData) * max_object_count,
                                                 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                 .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                 .debug_name = fmt::format("Objects Buffer {}", i),
                                             })
                              .value();

    frame.indirect_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(VkDrawIndirectCommand) * max_object_count,
                               .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Indirect Buffer {}", i),
                           })
            .value();

    frame.global_descriptor_set =
        descriptor_allocator_->allocate(global_descriptor_set_layout_).value();
    frame.object_descriptor_set =
        descriptor_allocator_->allocate(object_descriptor_set_layout_).value();

    const VkDescriptorBufferInfo camera_buffer_info = {
        .buffer = frame.camera_buffer.buffer, .offset = 0, .range = sizeof(GPUCameraData)};

    const VkWriteDescriptorSet camera_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .dstSet = frame.global_descriptor_set,
                                               .dstBinding = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                               .pBufferInfo = &camera_buffer_info};

    const VkDescriptorBufferInfo scene_buffer_info = {
        .buffer = scene_parameter_buffer_, .offset = 0, .range = sizeof(GPUSceneParameters)};
    const VkWriteDescriptorSet scene_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                              .dstSet = frame.global_descriptor_set,
                                              .dstBinding = 1,
                                              .descriptorCount = 1,
                                              .descriptorType =
                                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                              .pBufferInfo = &scene_buffer_info};

    const VkDescriptorImageInfo shadow_map_image_buffer_info = {
        .sampler = shadow_map_sampler_,
        .imageView = shadow_map_image_view_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkWriteDescriptorSet shadow_map_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                   .dstSet = frame.global_descriptor_set,
                                                   .dstBinding = 2,
                                                   .descriptorCount = 1,
                                                   .descriptorType =
                                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                   .pImageInfo = &shadow_map_image_buffer_info};

    const VkDescriptorBufferInfo object_buffer_info = {.buffer = frame.object_buffer.buffer,
                                                       .offset = 0,
                                                       .range = sizeof(GPUObjectData) *
                                                                max_object_count};

    const VkWriteDescriptorSet object_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .dstSet = frame.object_descriptor_set,
                                               .dstBinding = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                               .pBufferInfo = &object_buffer_info};

    VkWriteDescriptorSet set_writes[] = {camera_write, scene_write, shadow_map_write, object_write};

    vkUpdateDescriptorSets(context_, beyond::size(set_writes), set_writes, 0, nullptr);
  }
}

void Renderer::init_pipelines()
{
  ZoneScoped;

  init_mesh_pipeline();
  init_shadow_pipeline();
}

void Renderer::init_mesh_pipeline()
{
  const auto asset_path = Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
  const std::filesystem::path shader_directory = asset_path / "shaders";

  const ShaderHandle vertex_shader =
      pipeline_manager_->add_shader("mesh.vert.glsl", ShaderStage::vertex);
  const ShaderHandle fragment_shader =
      pipeline_manager_->add_shader("mesh.frag.glsl", ShaderStage::fragment);

  const VkDescriptorSetLayout set_layouts[] = {global_descriptor_set_layout_,
                                               object_descriptor_set_layout_, material_set_layout_};

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = beyond::size(set_layouts),
      .pSetLayouts = set_layouts,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr,
                                  &mesh_pipeline_layout_));

  // TODO: how to deal with specialized constant?
  //  struct ConstantData {
  //    int shadow_mode = 0;
  //  } constant_data;
  //
  //  VkSpecializationMapEntry map_entry = {
  //      .constantID = 0,
  //      .offset = 0,
  //      .size = sizeof(int),
  //  };
  //
  //  VkSpecializationInfo specialization_info = {
  //      .mapEntryCount = 1,
  //      .pMapEntries = &map_entry,
  //      .dataSize = sizeof(constant_data),
  //      .pData = &constant_data,
  //  };

  //  const VkPipelineShaderStageCreateInfo triangle_shader_stages[] = {
  //      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
  //       .stage = VK_SHADER_STAGE_VERTEX_BIT,
  //       .module = triangle_vert_shader,
  //       .pName = "main"},
  //      {
  //          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
  //          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
  //          .module = triangle_frag_shader,
  //          .pName = "main",
  //          .pSpecializationInfo = &specialization_info,
  //      }};

  std::vector<VkVertexInputBindingDescription> binding_descriptions = {
      {
          .binding = 0,
          .stride = sizeof(Vec3),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
      {
          .binding = 1,
          .stride = sizeof(Vec3),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
      {
          .binding = 2,
          .stride = sizeof(Vec2),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
      {
          .binding = 3,
          .stride = sizeof(Vec4),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      }};

  std::vector<VkVertexInputAttributeDescription> attribute_descriptions = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 1, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 2, .binding = 2, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
      {.location = 3, .binding = 3, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 0}};

  auto create_info = charlie::GraphicsPipelineCreateInfo{
      .layout = mesh_pipeline_layout_,
      .pipeline_rendering_create_info =
          vkh::PipelineRenderingCreateInfo{
              .color_attachment_formats = {swapchain_.image_format()},
              .depth_attachment_format = depth_format_,
          },
      .debug_name = "Mesh Graphics Pipeline",
      .vertex_input_state_create_info = {.binding_descriptions = binding_descriptions,
                                         .attribute_descriptions = attribute_descriptions},
      .shaders = {vertex_shader, fragment_shader},
      .cull_mode = VK_CULL_MODE_BACK_BIT};

  // constant_data.shadow_mode = 1;
  mesh_pipeline_ = pipeline_manager_->create_graphics_pipeline(create_info);
  // constant_data.shadow_mode = 0;
  create_info.debug_name = "Mesh Graphics Pipeline (without shadow)";
  mesh_pipeline_without_shadow_ = pipeline_manager_->create_graphics_pipeline(create_info);
}

void Renderer::init_shadow_pipeline()
{
  const ShaderHandle vertex_shader =
      pipeline_manager_->add_shader("shadow.vert.glsl", ShaderStage::vertex);

  const VkDescriptorSetLayout set_layouts[] = {global_descriptor_set_layout_,
                                               object_descriptor_set_layout_};

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = beyond::size(set_layouts),
      .pSetLayouts = set_layouts,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr,
                                  &shadow_map_pipeline_layout_));

  std::vector<VkVertexInputBindingDescription> binding_descriptions = {{
      .binding = 0,
      .stride = sizeof(Vec3),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }}; // Vertex input

  std::vector<VkVertexInputAttributeDescription> attribute_descriptions = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0}};

  shadow_map_pipeline_ = pipeline_manager_->create_graphics_pipeline(
      {.layout = shadow_map_pipeline_layout_,
       .pipeline_rendering_create_info =
           vkh::PipelineRenderingCreateInfo{
               .color_attachment_formats = {},
               .depth_attachment_format = depth_format_,
           },
       .debug_name = "Shadow Mapping Graphics Pipeline",
       .vertex_input_state_create_info = {.binding_descriptions = binding_descriptions,
                                          .attribute_descriptions = attribute_descriptions},
       .shaders = {vertex_shader},
       .cull_mode = VK_CULL_MODE_FRONT_BIT});
}

void Renderer::init_sampler()
{
  const VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                            .magFilter = VK_FILTER_LINEAR,
                                            .minFilter = VK_FILTER_LINEAR,
                                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT};
  vkCreateSampler(context_, &sampler_info, nullptr, &sampler_);
}

void Renderer::init_default_texture()
{
  CPUImage cpu_image{
      .name = "Default Albedo Texture Image",
      .width = 1,
      .height = 1,
      .compoments = 4,
      .data = std::make_unique_for_overwrite<uint8_t[]>(sizeof(uint8_t) * 4),
  };
  std::fill(cpu_image.data.get(), cpu_image.data.get() + 4, 255);

  const auto default_albedo_image = upload_image(cpu_image);

  VkImageView default_albedo_image_view =
      vkh::create_image_view(
          context(),
          {.image = default_albedo_image,
           .format = VK_FORMAT_R8G8B8A8_SRGB,
           .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
           .debug_name = "Default Albedo Texture Image View"})
          .value();

  default_albedo_texture_index =
      add_texture(Texture{.image = default_albedo_image, .image_view = default_albedo_image_view});

  CPUImage cpu_image2{
      .name = "Default Normal Texture Image",
      .width = 1,
      .height = 1,
      .compoments = 4,
      .data = std::make_unique_for_overwrite<uint8_t[]>(sizeof(uint8_t) * 4),
  };
  cpu_image2.data[0] = 127;
  cpu_image2.data[1] = 255;
  cpu_image2.data[2] = 127;
  cpu_image2.data[3] = 255;

  const auto default_normal_image = upload_image(cpu_image2, {
                                                                 .format = VK_FORMAT_R8G8B8A8_UNORM,
                                                             });
  VkImageView default_normal_image_view =
      vkh::create_image_view(
          context(),
          {.image = default_normal_image,
           .format = VK_FORMAT_R8G8B8A8_UNORM,
           .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
           .debug_name = "Default Normal Texture Image View"})
          .value();

  default_normal_texture_index =
      add_texture(Texture{.image = default_normal_image, .image_view = default_normal_image_view});
}

void Renderer::render(const charlie::Camera& camera)
{
  ZoneScopedN("Render");

  pipeline_manager_->update();

  const auto& frame = current_frame();
  constexpr u64 one_second = 1'000'000'000;

  u32 swapchain_image_index = 0;
  {
    ZoneScopedN("vkAcquireNextImageKHR");
    const VkResult result = vkAcquireNextImageKHR(
        context_, swapchain_, one_second, frame.present_semaphore, nullptr, &swapchain_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { return; }
    VK_CHECK(result);
  }

  // wait until the GPU has finished rendering the last frame.
  {
    ZoneScopedN("wait for render fence");
    VK_CHECK(vkWaitForFences(context_, 1, &frame.render_fence, true, one_second));
  }

  VK_CHECK(vkResetFences(context_, 1, &frame.render_fence));
  VK_CHECK(vkResetCommandBuffer(frame.main_command_buffer, 0));

  imgui_render_pass_->pre_render();

  VkCommandBuffer cmd = frame.main_command_buffer;

  const auto current_swapchain_image = swapchain_.images()[swapchain_image_index];
  const auto current_swapchain_image_view = swapchain_.image_views()[swapchain_image_index];

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  {
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    {
      TracyVkCollect(frame.tracy_vk_ctx, cmd);
      TracyVkZone(frame.tracy_vk_ctx, cmd, "Swapchain");

      transit_current_swapchain_image_for_rendering(cmd, current_swapchain_image);

      {
        // Camera
        const Mat4 view = camera.view_matrix();
        const Mat4 projection = camera.proj_matrix();

        const GPUCameraData cam_data = {
            .view = view,
            .proj = projection,
            .view_proj = projection * view,
        };

        const vkh::AllocatedBuffer& camera_buffer = current_frame().camera_buffer;

        void* data = nullptr;
        vmaMapMemory(context_.allocator(), camera_buffer.allocation, &data);
        memcpy(data, &cam_data, sizeof(GPUCameraData));
        vmaUnmapMemory(context_.allocator(), camera_buffer.allocation);

        // Scene data
        const auto dir = Vec3(scene_parameters_.sunlight_direction.xyz);
        const Vec3 up =
            abs(dot(dir, Vec3(0.0, 1.0, 0.0))) < 0.01 ? Vec3(0.0, 0.0, 1.0) : Vec3(0.0, 1.0, 0.0);

        scene_parameters_.sunlight_view_proj =
            beyond::ortho(-1.f, 1.f, 1.f, -1.f, 0.f, 10.f) * beyond::look_at(-dir, Vec3(0.0), up);

        char* scene_data = nullptr;
        vmaMapMemory(context_.allocator(), scene_parameter_buffer_.allocation, (void**)&scene_data);

        const size_t frame_index = frame_number_ % frame_overlap;

        scene_data += context_.align_uniform_buffer_size(sizeof(GPUSceneParameters)) * frame_index;
        memcpy(scene_data, &scene_parameters_, sizeof(GPUSceneParameters));
        vmaUnmapMemory(context_.allocator(), scene_parameter_buffer_.allocation);
      }

      if (enable_shadow_mapping) { draw_shadow(cmd); }

      draw_scene(cmd, current_swapchain_image_view);

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

  present(swapchain_image_index);

  ++frame_number_;
}

void Renderer::present(u32& swapchain_image_index)
{
  ZoneScopedN("vkQueuePresentKHR");

  const auto& frame = current_frame();

  VkSwapchainKHR swapchain = swapchain_.get();
  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame.render_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &swapchain_image_index,
  };

  const VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // Supress
    return;
  }
  VK_CHECK(result);
}

[[nodiscard]] auto Renderer::upload_mesh_data(const CPUMesh& cpu_mesh) -> MeshHandle
{
  static constexpr auto buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  const vkh::AllocatedBuffer position_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.positions, buffer_usage,
                    fmt::format("{} Position", cpu_mesh.name))
          .value();
  const vkh::AllocatedBuffer normal_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.normals, buffer_usage,
                    fmt::format("{} Normal", cpu_mesh.name))
          .value();
  const vkh::AllocatedBuffer uv_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.uv, buffer_usage,
                    fmt::format("{} Texcoord", cpu_mesh.name))
          .value();

  const vkh::AllocatedBuffer tangent_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.tangents, buffer_usage,
                    fmt::format("{} Tangent", cpu_mesh.name))
          .value();

  const vkh::AllocatedBuffer index_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    fmt::format("{} Index", cpu_mesh.name))
          .value();

  return meshes_.insert(Mesh{.position_buffer = position_buffer,
                             .normal_buffer = normal_buffer,
                             .uv_buffer = uv_buffer,
                             .tangent_buffer = tangent_buffer,
                             .index_buffer = index_buffer,
                             .vertices_count = beyond::narrow<u32>(cpu_mesh.positions.size()),
                             .index_count = beyond::narrow<u32>(cpu_mesh.indices.size())});
}

void Renderer::draw_shadow(VkCommandBuffer cmd)
{
  ZoneScopedN("Shadow Render Pass");
  TracyVkZone(current_frame().tracy_vk_ctx, cmd, "Shadow Render Pass");

  vkh::cmd_pipeline_barrier2(
      cmd,
      {.image_barriers = std::array{
           vkh::ImageBarrier2{
               .stage_masks = {VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
               .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
               .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
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
  const size_t frame_index = frame_number_ % frame_overlap;
  const u32 uniform_offset =
      narrow<u32>(context_.align_uniform_buffer_size(sizeof(GPUSceneParameters))) *
      narrow<u32>(frame_index);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout_, 0, 1,
                          &current_frame().global_descriptor_set, 1, &uniform_offset);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_manager_->get_pipeline(shadow_map_pipeline_));
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout_, 1, 1,
                          &current_frame().object_descriptor_set, 0, nullptr);

  auto* object_data =
      beyond::narrow<GPUObjectData*>(context_.map(current_frame().object_buffer).value());
  const usize object_count = scene_->global_transforms.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (usize i = 0; i < object_count; ++i) { object_data[i].model = scene_->global_transforms[i]; }

  context_.unmap(current_frame().object_buffer);

  for (const auto [node_index, render_component] : scene_->render_components) {
    const MeshHandle mesh_handle = render_component.mesh;
    const auto& mesh = meshes_.try_get(mesh_handle).expect("Cannot find mesh by handle");

    static constexpr VkDeviceSize vertex_offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.position_buffer.buffer, &vertex_offset);
    vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, node_index);
  }

  vkCmdEndRendering(cmd);

  vkh::cmd_pipeline_barrier2(
      cmd, {.image_barriers = std::array{
                vkh::ImageBarrier2{.stage_masks = {VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
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

void Renderer::draw_scene(VkCommandBuffer cmd, VkImageView current_swapchain_image_view)
{
  ZoneScopedN("Mesh Render Pass");
  TracyVkZone(current_frame().tracy_vk_ctx, cmd, "Mesh Render Pass");

  const VkRenderingAttachmentInfo color_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = current_swapchain_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = VkClearValue{.color = {.float32 = {0.5f, 0.5f, 0.5f, 1.0f}}},
  };

  const VkRenderingAttachmentInfo depth_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = depth_image_view_,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = VkClearValue{.depthStencil = {.depth = 1.f}},
  };

  const VkRenderingInfo render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .offset =
                  {
                      .x = 0,
                      .y = 0,
                  },
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

  // Render objects
  render_objects_.clear();
  for (const auto [node_index, render_component] : scene_->render_components) {
    render_objects_.push_back(RenderObject{
        .mesh = render_component.mesh,
        .material = render_component.material,
        .model_matrix = scene_->global_transforms[node_index],
    });
  }

  // Copy to object buffer
  auto* object_data =
      beyond::narrow<GPUObjectData*>(context_.map(current_frame().object_buffer).value());

  const usize object_count = render_objects_.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (usize i = 0; i < scene_->global_transforms.size(); ++i) {
    object_data[i].model = scene_->global_transforms[i];
  }

  context_.unmap(current_frame().object_buffer);

  // Generate draws
  const auto draws = compact_draws(render_objects_);

  {
    auto* draw_commands =
        context_.map<VkDrawIndexedIndirectCommand>(current_frame().indirect_buffer).value();
    BEYOND_ENSURE(draw_commands != nullptr);

    for (u32 i = 0; i < object_count; ++i) {
      const auto mesh_handle = render_objects_[i].mesh;
      const auto& mesh = meshes_.try_get(mesh_handle).expect("Cannot find mesh by handle");

      draw_commands[i] = VkDrawIndexedIndirectCommand{
          .indexCount = mesh.index_count,
          .instanceCount = 1,
          .firstIndex = 0,
          .vertexOffset = 0,
          .firstInstance = i,
      };
    }
    context_.unmap(current_frame().indirect_buffer);
  }

  if (enable_shadow_mapping) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_manager_->get_pipeline(mesh_pipeline_));
  } else {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_manager_->get_pipeline(mesh_pipeline_without_shadow_));
  }

  const u32 uniform_offset =
      beyond::narrow<u32>(context_.align_uniform_buffer_size(sizeof(GPUSceneParameters))) *
      beyond::narrow<u32>(frame_index);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 0, 1,
                          &current_frame().global_descriptor_set, 1, &uniform_offset);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 1, 1,
                          &current_frame().object_descriptor_set, 0, nullptr);
  for (const IndirectBatch& draw : draws) {
    const auto& material = materials_.try_get(draw.material);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 2, 1,
                            &material.value().descriptor_set, 0, nullptr);

    const Mesh& mesh = meshes_.try_get(draw.mesh).expect("Cannot find mesh by handle!");
    constexpr VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.position_buffer.buffer, &offset);
    vkCmdBindVertexBuffers(cmd, 1, 1, &mesh.normal_buffer.buffer, &offset);
    vkCmdBindVertexBuffers(cmd, 2, 1, &mesh.uv_buffer.buffer, &offset);
    vkCmdBindVertexBuffers(cmd, 3, 1, &mesh.tangent_buffer.buffer, &offset);

    vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    const VkDeviceSize indirect_offset = draw.first * sizeof(VkDrawIndexedIndirectCommand);
    constexpr uint32_t draw_stride = sizeof(VkDrawIndexedIndirectCommand);
    vkCmdDrawIndexedIndirect(cmd, current_frame().indirect_buffer, indirect_offset, draw.count,
                             draw_stride);
  }

  vkCmdEndRendering(cmd);
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  imgui_render_pass_ = nullptr;

  vkDestroySampler(context_, sampler_, nullptr);

  for (auto texture : textures_) { vkDestroyImageView(context_, texture.image_view, nullptr); }
  for (auto image : images_) { vkh::destroy_image(context_, image); }

  for (const auto& mesh : meshes_.values()) { destroy_mesh(context_, mesh); }

  vkDestroyCommandPool(context_, upload_context_.command_pool, nullptr);
  vkDestroyFence(context_, upload_context_.fence, nullptr);

  vkDestroyPipelineLayout(context_, mesh_pipeline_layout_, nullptr);

  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);

  vkDestroyImageView(context_, shadow_map_image_view_, nullptr);
  vkh::destroy_image(context_, shadow_map_image_);
  vkDestroySampler(context_, shadow_map_sampler_, nullptr);

  descriptor_allocator_ = nullptr;
  descriptor_layout_cache_ = nullptr;

  vkh::destroy_buffer(context_, scene_parameter_buffer_);
  for (auto& frame : frames_) {
    TracyVkDestroy(frame.tracy_vk_ctx);

    vkh::destroy_buffer(context_, frame.indirect_buffer);
    vkh::destroy_buffer(context_, frame.object_buffer);
    vkh::destroy_buffer(context_, frame.camera_buffer);

    vkDestroyFence(context_, frame.render_fence, nullptr);
    vkDestroySemaphore(context_, frame.render_semaphore, nullptr);
    vkDestroySemaphore(context_, frame.present_semaphore, nullptr);
    vkDestroyCommandPool(context_, frame.command_pool, nullptr);
  }
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
}

auto Renderer::add_texture(Texture texture) -> uint32_t
{
  textures_.push_back(texture);
  return beyond::narrow<uint32_t>(textures_.size() - 1);
}

auto Renderer::create_material(const CPUMaterial& material_info) -> MaterialHandle
{
  const auto albedo_texture = textures_.at(material_info.albedo_texture_index.value());
  const auto normal_texture = textures_.at(material_info.normal_texture_index.value());
  const auto occlusion_texture = textures_.at(material_info.occlusion_texture_index.value());

  // alloc descriptor set for material
  VkDescriptorSet material_descriptor_set =
      descriptor_allocator_->allocate(material_set_layout_).value();

  // write to the descriptor set so that it points to diffuse texture
  const VkDescriptorImageInfo albedo_image_buffer_info = {
      .sampler = sampler_,
      .imageView = albedo_texture.image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const VkDescriptorImageInfo normal_image_buffer_info = {
      .sampler = sampler_,
      .imageView = normal_texture.image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const VkDescriptorImageInfo occlusion_image_buffer_info = {
      .sampler = sampler_,
      .imageView = occlusion_texture.image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const VkWriteDescriptorSet albedo_texture_write = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = material_descriptor_set,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &albedo_image_buffer_info,
  };
  const VkWriteDescriptorSet normal_texture_write = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = material_descriptor_set,
      .dstBinding = 1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &normal_image_buffer_info,
  };
  const VkWriteDescriptorSet occlusion_texture_write = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = material_descriptor_set,
      .dstBinding = 2,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &occlusion_image_buffer_info,
  };
  const VkWriteDescriptorSet texture_write_sets[] = {albedo_texture_write, normal_texture_write,
                                                     occlusion_texture_write};
  vkUpdateDescriptorSets(context_, beyond::size(texture_write_sets), texture_write_sets, 0,
                         nullptr);

  return materials_.insert(Material{.descriptor_set = material_descriptor_set});
}

} // namespace charlie