#include "renderer.hpp"

#include "../vulkan_helpers/graphics_pipeline.hpp"
#include "../vulkan_helpers/initializers.hpp"
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

static constexpr uint32_t max_bindless_texture_count = 1024;
static constexpr uint32_t bindless_texture_binding = 10;

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
  beyond::Vec3 position;
};

constexpr beyond::usize max_object_count = 10000;

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

static void cmd_generate_mipmap(VkCommandBuffer cmd, VkImage image, Resolution image_resolution,
                                u32 mip_levels)
{
  vkh::ImageBarrier2 barrier{.image = image,
                             .subresource_range = {
                                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1,
                             }};

  i32 mip_width = narrow<i32>(image_resolution.width);
  i32 mip_height = narrow<i32>(image_resolution.height);

  for (u32 i = 1; i < mip_levels; i++) {
    barrier.subresource_range.baseMipLevel = i - 1;
    barrier.layouts = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    barrier.access_masks = {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
    barrier.stage_masks = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    vkh::cmd_pipeline_barrier2(cmd, {.image_barriers = std::array{barrier.to_vk_struct()}});

    const i32 next_mip_width = std::max(mip_width / 2, 1);
    const i32 next_mip_height = std::max(mip_height / 2, 1);

    VkImageBlit blit{.srcSubresource =
                         {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .mipLevel = i - 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                         },
                     .srcOffsets = {{0, 0, 0}, {mip_width, mip_height, 1}},
                     .dstSubresource =
                         {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .mipLevel = i,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                         },
                     .dstOffsets = {{0, 0, 0}, {next_mip_width, next_mip_height, 1}}};
    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    barrier.layouts = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    barrier.access_masks = {VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT};
    barrier.stage_masks = {VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT};
    vkh::cmd_pipeline_barrier2(cmd, {.image_barriers = std::array{barrier.to_vk_struct()}});

    mip_width = next_mip_width;
    mip_height = next_mip_height;
  }

  barrier.subresource_range.baseMipLevel = mip_levels - 1;
  barrier.layouts = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  barrier.access_masks = {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT};
  barrier.stage_masks = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT};
  vkh::cmd_pipeline_barrier2(cmd, {.image_barriers = std::array{barrier.to_vk_struct()}});
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

  const u32 mip_levels = upload_info.mip_levels;
  const bool need_generate_mipmap = mip_levels > 1;

  const auto image_debug_name =
      cpu_image.name.empty() ? fmt::format("{} Image", cpu_image.name) : "Image";
  vkh::AllocatedImage allocated_image =
      vkh::create_image(context,
                        vkh::ImageCreateInfo{
                            .format = upload_info.format,
                            .extent = image_extent,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            .mip_levels = mip_levels,
                            .debug_name = image_debug_name,
                        })
          .expect("Failed to create image");

  immediate_submit(context, upload_context, [&](VkCommandBuffer cmd) {
    const VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
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
    if (not need_generate_mipmap) {
      vkh::cmd_pipeline_barrier2(
          cmd, {.image_barriers = std::array{
                    vkh::ImageBarrier2{.stage_masks = {VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                       VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
                                       .access_masks = {VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                        VK_ACCESS_2_SHADER_READ_BIT},
                                       .layouts = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                       .image = allocated_image.image,
                                       .subresource_range = range}
                        .to_vk_struct() //
                }});
    } else {
      VkFormatProperties format_properties;
      vkGetPhysicalDeviceFormatProperties(context_.physical_device(), upload_info.format,
                                          &format_properties);
      BEYOND_ENSURE(format_properties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

      cmd_generate_mipmap(cmd, allocated_image.image, Resolution{cpu_image.width, cpu_image.height},
                          mip_levels);
    }
  });

  return images_.emplace_back(allocated_image).image;
}

void Renderer::init_shadow_map()
{
  ZoneScoped;

  for (u32 i = 0; i < frame_overlap; ++i) {
    shadow_map_images_[i] =
        vkh::create_image(
            context_,
            vkh::ImageCreateInfo{
                .format = depth_format_,
                .extent = VkExtent3D{shadow_map_width_, shadow_map_height_, 1},
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .debug_name = "Shadow Map Image",
            })
            .expect("Fail to create shadow map image");
    shadow_map_image_views_[i] =
        vkh::create_image_view(
            context_, vkh::ImageViewCreateInfo{.image = shadow_map_images_[i].image,
                                               .format = depth_format_,
                                               .subresource_range =
                                                   vkh::SubresourceRange{
                                                       .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                   },
                                               .debug_name = "Shadow Map Image View"})
            .expect("Fail to create shadow map image view");
  }

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

  static constexpr VkDescriptorSetLayoutBinding texture_bindings[] = {
      // Image sampler binding
      {.binding = bindless_texture_binding,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = max_bindless_texture_count,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

  static constexpr VkDescriptorBindingFlags bindless_flags =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; // VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
  static constexpr VkDescriptorBindingFlags binding_flags[] = {bindless_flags};

  VkDescriptorSetLayoutBindingFlagsCreateInfo layout_binding_flags_create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .bindingCount = 1,
      .pBindingFlags = binding_flags,
  };

  static constexpr VkDescriptorSetLayoutBinding material_bindings[] = {
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  material_set_layout_ = descriptor_layout_cache_
                             ->create_descriptor_set_layout({
                                 .p_next = nullptr,
                                 .bindings = material_bindings,
                             })
                             .value();

  {
    vkh::DescriptorSetLayoutCreateInfo bindless_texture_descriptor_set_layout_create_info{
        .p_next = &layout_binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindings = texture_bindings,
        .debug_name = "Material Descriptor Set Layout",
    };

    static constexpr VkDescriptorPoolSize pool_sizes_bindless[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_bindless_texture_count},
    };
    bindless_texture_set_layout_ = vkh::create_descriptor_set_layout(
                                       context_, bindless_texture_descriptor_set_layout_create_info)
                                       .value();
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = max_bindless_texture_count * beyond::size(pool_sizes_bindless),
        .poolSizeCount = beyond::size(pool_sizes_bindless),
        .pPoolSizes = pool_sizes_bindless,
    };
    VK_CHECK(vkCreateDescriptorPool(context_, &descriptor_pool_create_info, nullptr,
                                    &bindless_texture_descriptor_pool_));

    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = bindless_texture_descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &bindless_texture_set_layout_};

    VK_CHECK(vkAllocateDescriptorSets(context_, &alloc_info, &bindless_texture_descriptor_set_));
  }

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

    frame.model_matrix_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(Mat4) * max_object_count,
                               .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Objects Buffer {}", i),
                           })
            .value();

    frame.material_index_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(int) * max_object_count,
                               .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Material Index Buffer {}", i),
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

    // Global
    const VkDescriptorBufferInfo camera_buffer_info = {
        .buffer = frame.camera_buffer.buffer, .offset = 0, .range = sizeof(GPUCameraData)};
    const VkDescriptorBufferInfo scene_buffer_info = {
        .buffer = scene_parameter_buffer_, .offset = 0, .range = sizeof(GPUSceneParameters)};

    const VkDescriptorImageInfo shadow_map_image_info = {
        .sampler = shadow_map_sampler_,
        .imageView = shadow_map_image_views_[i],
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    auto global_descriptor_build_result =
        DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
            .bind_buffer(0, camera_buffer_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(1, scene_buffer_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(2, shadow_map_image_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .build()
            .value();
    global_descriptor_set_layout_ = global_descriptor_build_result.layout;
    frame.global_descriptor_set = global_descriptor_build_result.set;

    // Objects
    const VkDescriptorBufferInfo model_matrix_buffer_info = {
        .buffer = frame.model_matrix_buffer.buffer,
        .offset = 0,
        .range = sizeof(Mat4) * max_object_count};
    const VkDescriptorBufferInfo material_index_buffer_info = {
        .buffer = frame.material_index_buffer.buffer,
        .offset = 0,
        .range = sizeof(int) * max_object_count};

    auto objects_descriptor_build_result =
        DescriptorBuilder{*descriptor_layout_cache_, *descriptor_allocator_} //
            .bind_buffer(0, model_matrix_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, material_index_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT)
            .build()
            .value();
    object_descriptor_set_layout_ = objects_descriptor_build_result.layout;
    frame.object_descriptor_set = objects_descriptor_build_result.set;
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
  ZoneScoped;

  const auto asset_path = Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
  const std::filesystem::path shader_directory = asset_path / "shaders";

  const ShaderHandle vertex_shader =
      pipeline_manager_->add_shader("mesh.vert.glsl", ShaderStage::vertex);
  const ShaderHandle fragment_shader =
      pipeline_manager_->add_shader("mesh.frag.glsl", ShaderStage::fragment);

  const VkDescriptorSetLayout set_layouts[] = {global_descriptor_set_layout_,
                                               object_descriptor_set_layout_, material_set_layout_,
                                               bindless_texture_set_layout_};

  const VkPushConstantRange push_constant = {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(MeshPushConstant),
  };

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = beyond::size(set_layouts),
      .pSetLayouts = set_layouts,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr,
                                  &mesh_pipeline_layout_));

  struct ConstantData {
    int shadow_mode = 0;
  } constant_data;

  VkSpecializationMapEntry map_entry = {
      .constantID = 0,
      .offset = 0,
      .size = sizeof(int),
  };

  VkSpecializationInfo specialization_info = {
      .mapEntryCount = 1,
      .pMapEntries = &map_entry,
      .dataSize = sizeof(constant_data),
      .pData = &constant_data,
  };

  auto create_info = charlie::GraphicsPipelineCreateInfo{
      .layout = mesh_pipeline_layout_,
      .pipeline_rendering_create_info =
          vkh::PipelineRenderingCreateInfo{
              .color_attachment_formats = {swapchain_.image_format()},
              .depth_attachment_format = depth_format_,
          },
      .debug_name = "Mesh Graphics Pipeline",
      .stages = {{vertex_shader}, {fragment_shader, &specialization_info}},
      .rasterization_state = {.cull_mode = VK_CULL_MODE_BACK_BIT}};

  constant_data.shadow_mode = 1;
  mesh_pipeline_ = pipeline_manager_->create_graphics_pipeline(create_info);
  constant_data.shadow_mode = 0;
  create_info.debug_name = "Mesh Graphics Pipeline (without shadow)";
  mesh_pipeline_without_shadow_ = pipeline_manager_->create_graphics_pipeline(create_info);
}

void Renderer::init_shadow_pipeline()
{
  ZoneScoped;

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
       .stages = {{vertex_shader}},
       .rasterization_state = {
           .depth_bias_info = DepthBiasInfo{.constant_factor = 1.25f, .slope_factor = 1.75f}}});
}

void Renderer::init_sampler()
{
  const VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                            .magFilter = VK_FILTER_LINEAR,
                                            .minFilter = VK_FILTER_LINEAR,
                                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .maxLod = VK_LOD_CLAMP_NONE};
  vkCreateSampler(context_, &sampler_info, nullptr, &default_sampler_);
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
  cpu_image2.data[1] = 127;
  cpu_image2.data[2] = 255;
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

void Renderer::update(const charlie::Camera& camera)
{
  ZoneScopedN("Update");

  pipeline_manager_->update();

  update_textures();

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

    scene_parameters_.sunlight_view_proj = beyond::ortho(-10.f, 10.f, 10.f, -10.f, -100.f, 100.f) *
                                           beyond::look_at(-dir, Vec3(0.0), up);

    char* scene_data = nullptr;
    vmaMapMemory(context_.allocator(), scene_parameter_buffer_.allocation, (void**)&scene_data);

    const size_t frame_index = frame_number_ % frame_overlap;

    scene_data += context_.align_uniform_buffer_size(sizeof(GPUSceneParameters)) * frame_index;
    memcpy(scene_data, &scene_parameters_, sizeof(GPUSceneParameters));
    vmaUnmapMemory(context_.allocator(), scene_parameter_buffer_.allocation);
  }
}

void Renderer::render(const charlie::Camera& camera)
{
  ZoneScopedN("Render");

  update(camera);

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

  current_frame_deletion_queue().flush();

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

  present(beyond::ref(swapchain_image_index));

  ++frame_number_;
}

void Renderer::present(beyond::Ref<u32> swapchain_image_index)
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
      .pImageIndices = &swapchain_image_index.get(),
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
  std::vector<SubMesh> submeshes;
  for (usize i = 0; i < cpu_mesh.submeshes.size(); ++i) {
    const auto& submesh = cpu_mesh.submeshes[i];
    static constexpr auto vertex_buffer_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const vkh::AllocatedBuffer position_buffer =
        upload_buffer(context_, upload_context_, submesh.positions,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | vertex_buffer_usage,
                      fmt::format("{} Position ({})", cpu_mesh.name, i))
            .value();
    const vkh::AllocatedBuffer vertex_buffer =
        upload_buffer(context_, upload_context_, submesh.vertices, vertex_buffer_usage,
                      fmt::format("{} Vertex ({})", cpu_mesh.name, i))
            .value();
    const vkh::AllocatedBuffer index_buffer =
        upload_buffer(context_, upload_context_, submesh.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      fmt::format("{} Index ({})", cpu_mesh.name, i))
            .value();

    submeshes.push_back(SubMesh{.position_buffer = position_buffer,
                                .vertex_buffer = vertex_buffer,
                                .index_buffer = index_buffer,
                                .vertices_count = narrow<u32>(submesh.positions.size()),
                                .index_count = narrow<u32>(submesh.indices.size()),
                                .material_index = submesh.material_index.value()});
  }

  return meshes_.insert(Mesh{.submeshes = std::move(submeshes)});
}

void Renderer::draw_shadow(VkCommandBuffer cmd)
{
  ZoneScopedN("Shadow Render Pass");
  TracyVkZone(current_frame().tracy_vk_ctx, cmd, "Shadow Render Pass");

  const u32 current_frame_index = frame_number_ % frame_overlap;

  vkh::cmd_pipeline_barrier2(
      cmd,
      {.image_barriers = std::array{
           vkh::ImageBarrier2{
               .stage_masks = {VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
               .access_masks = {VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
               .layouts = {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
               .image = shadow_map_images_[current_frame_index].image,
               .subresource_range =
                   vkh::SubresourceRange{
                       .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                   }}
               .to_vk_struct() //
       }});

  const VkRenderingAttachmentInfo depth_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = shadow_map_image_views_[current_frame_index],
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

  // TODO: wrong model matrix index
  auto* object_model_matrix_data =
      beyond::narrow<Mat4*>(context_.map(current_frame().model_matrix_buffer).value());
  const usize object_count = scene_->global_transforms.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (usize i = 0; i < object_count; ++i) {
    object_model_matrix_data[i] = scene_->global_transforms[i];
  }
  context_.unmap(current_frame().model_matrix_buffer);

  for (const auto [node_index, render_component] : scene_->render_components) {
    const MeshHandle mesh_handle = render_component.mesh;
    const auto& mesh = meshes_.try_get(mesh_handle).expect("Cannot find mesh by handle");

    static constexpr VkDeviceSize vertex_offset = 0;
    for (const auto& submesh : mesh.submeshes) {
      vkCmdBindVertexBuffers(cmd, 0, 1, &submesh.position_buffer.buffer, &vertex_offset);
      vkCmdBindIndexBuffer(cmd, submesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(cmd, submesh.index_count, 1, 0, 0, node_index);
    }
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
                                   .image = shadow_map_images_[current_frame_index].image,
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
      .clearValue = VkClearValue{.color = {.float32 = {0.8f, 0.8f, 0.8f, 1.0f}}},
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

  // Render objects
  draws_.clear();
  for (const auto [node_index, render_component] : scene_->render_components) {
    const Mesh& mesh = meshes_.try_get(render_component.mesh).expect("Cannot find mesh by handle!");

    for (const auto& submesh : mesh.submeshes) {
      draws_.push_back(RenderObject{.submesh = &submesh,
                                    .model_matrix = scene_->global_transforms.at(node_index)});
    }
  }

  // Copy to object buffer
  auto* object_model_matrix_data =
      beyond::narrow<Mat4*>(context_.map(current_frame().model_matrix_buffer).value());
  auto* object_material_index_data =
      beyond::narrow<i32*>(context_.map(current_frame().material_index_buffer).value());

  const usize object_count = draws_.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (usize i = 0; i < draws_.size(); ++i) {
    const RenderObject& render_object = draws_[i];
    object_model_matrix_data[i] = render_object.model_matrix;
    object_material_index_data[i] = narrow<i32>(render_object.submesh->material_index);
  }

  context_.unmap(current_frame().model_matrix_buffer);
  context_.unmap(current_frame().material_index_buffer);

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
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 2, 1,
                          &material_descriptor_set_, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 3, 1,
                          &bindless_texture_descriptor_set_, 0, nullptr);

  for (u32 i = 0; i < draws_.size(); ++i) {
    const SubMesh& submesh = *draws_[i].submesh;
    constexpr auto get_buffer_device_address = [](VkDevice device, VkBuffer buffer) {
      VkBufferDeviceAddressInfo device_address_info{
          .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
      return vkGetBufferDeviceAddress(device, &device_address_info);
    };

    const VkDeviceAddress pos_buffer_address =
        get_buffer_device_address(context_, submesh.position_buffer.buffer);
    const VkDeviceAddress vertex_buffer_address =
        get_buffer_device_address(context_, submesh.vertex_buffer.buffer);

    const MeshPushConstant push_constant{
        .position_buffer_address = pos_buffer_address,
        .vertex_buffer_address = vertex_buffer_address,
    };
    vkCmdPushConstants(cmd, mesh_pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshPushConstant), &push_constant);

    vkCmdBindIndexBuffer(cmd, submesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, submesh.index_count, 1, 0, 0, i);
  }

  vkCmdEndRendering(cmd);
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  imgui_render_pass_ = nullptr;

  vkDestroySampler(context_, default_sampler_, nullptr);

  for (auto texture : textures_) { vkDestroyImageView(context_, texture.image_view, nullptr); }
  for (auto image : images_) { vkh::destroy_image(context_, image); }

  for (auto& mesh : meshes_.values()) {
    for (auto& submesh : mesh.submeshes) { destroy_submesh(context_, submesh); }
  }

  vkDestroyCommandPool(context_, upload_context_.command_pool, nullptr);
  vkDestroyFence(context_, upload_context_.fence, nullptr);

  vkDestroyPipelineLayout(context_, mesh_pipeline_layout_, nullptr);

  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);

  for (u32 i = 0; i < frame_overlap; ++i) {
    vkDestroyImageView(context_, shadow_map_image_views_[i], nullptr);
    vkh::destroy_image(context_, shadow_map_images_[i]);
  }

  vkDestroySampler(context_, shadow_map_sampler_, nullptr);

  descriptor_allocator_ = nullptr;
  descriptor_layout_cache_ = nullptr;

  vkh::destroy_buffer(context_, scene_parameter_buffer_);
  for (auto& frame : frames_) {
    TracyVkDestroy(frame.tracy_vk_ctx);

    vkh::destroy_buffer(context_, frame.indirect_buffer);
    vkh::destroy_buffer(context_, frame.model_matrix_buffer);
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

auto Renderer::add_texture(Texture texture) -> u32
{
  textures_.push_back(texture);
  const u32 texture_index = narrow<u32>(textures_.size() - 1);

  textures_to_update_.push_back(TextureUpdate{
      .index = texture_index,
  });

  return texture_index;
}

auto Renderer::add_material(const CPUMaterial& material_info) -> u32
{
  const u32 albedo_texture_index =
      material_info.albedo_texture_index.value_or(default_albedo_texture_index);
  const u32 normal_texture_index =
      material_info.normal_texture_index.value_or(default_normal_texture_index);
  // TODO: Can I use default albedo for metallic roughness?
  const u32 metallic_roughness_texture_index =
      material_info.metallic_roughness_texture_index.value_or(default_albedo_texture_index);
  const u32 occlusion_texture_index =
      material_info.occlusion_texture_index.value_or(default_albedo_texture_index);

  materials_.push_back(Material{
      .base_color_factor = material_info.base_color_factor,
      .albedo_texture_index = albedo_texture_index,
      .normal_texture_index = normal_texture_index,
      .metallic_roughness_texture_index = metallic_roughness_texture_index,
      .occlusion_texture_index = occlusion_texture_index,
      .metallic_factor = material_info.metallic_factor,
      .roughness_factor = material_info.roughness_factor,
  });

  return narrow<u32>(materials_.size() - 1);
}

void Renderer::upload_materials()
{
  material_buffer_ = vkh::create_buffer_from_data(context_,
                                                  vkh::BufferCreateInfo{
                                                      .size = sizeof(Material) * materials_.size(),
                                                      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                      .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                      .debug_name = fmt::format("Material Buffer"),
                                                  },
                                                  materials_.data())
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
  BEYOND_ENSURE(material_set_layout_ == result.layout);
  material_descriptor_set_ = result.set;
}

void Renderer::update_textures()
{
  beyond::StaticVector<VkDescriptorImageInfo, max_bindless_texture_count> image_infos;
  beyond::StaticVector<VkWriteDescriptorSet, max_bindless_texture_count> descriptor_writes;

  for (const TextureUpdate& texture_to_update : textures_to_update_) {
    const Texture& texture = textures_.at(texture_to_update.index);

    VkDescriptorImageInfo& image_info = image_infos.emplace_back(VkDescriptorImageInfo{
        .sampler = texture.sampler == VK_NULL_HANDLE ? default_sampler_ : texture.sampler,
        .imageView = texture.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    descriptor_writes.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bindless_texture_descriptor_set_,
        .dstBinding = bindless_texture_binding,
        .dstArrayElement = texture_to_update.index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    });
  }
  textures_to_update_.clear();

  vkUpdateDescriptorSets(context_, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

} // namespace charlie