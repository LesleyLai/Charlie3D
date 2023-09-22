#include "renderer.hpp"

#include "vulkan_helpers/commands.hpp"
#include "vulkan_helpers/descriptor_pool.hpp"
#include "vulkan_helpers/descriptor_utils.hpp"
#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/image_view.hpp"
#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/sync.hpp"

#include "shader_compiler/shader_compiler.hpp"

#include <cstddef>

#include <spdlog/spdlog.h>
#include <stb_image.h>

#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/utils/defer.hpp>

#include "camera.hpp"
#include "mesh.hpp"

#include "imgui_render_pass.hpp"

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace {

struct GPUObjectData {
  beyond::Mat4 model;
};

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
};

constexpr std::size_t max_object_count = 10000;

[[nodiscard]] auto
sampler_create_info(VkFilter filters,
                    VkSamplerAddressMode address_mode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
    -> VkSamplerCreateInfo
{
  return VkSamplerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .magFilter = filters,
      .minFilter = filters,
      .addressModeU = address_mode,
      .addressModeV = address_mode,
      .addressModeW = address_mode,
  };
}

auto write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
                            const VkDescriptorImageInfo* image_info, uint32_t binding)
    -> VkWriteDescriptorSet
{
  return VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = dstSet,
      .dstBinding = binding,
      .descriptorCount = 1,
      .descriptorType = type,
      .pImageInfo = image_info,
  };
}

[[nodiscard]] auto load_image_from_file(charlie::Renderer& renderer, const char* filename)
    -> beyond::optional<vkh::AllocatedImage>
{
  ZoneScoped;

  vkh::Context& context = renderer.context();

  int tex_width{}, tex_height{}, tex_channels{};
  stbi_uc* pixels = stbi_load(filename, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (!pixels) {
    fmt::print(stderr, "Failed to load texture file {}\n", filename);
    std::fflush(stderr);
    return beyond::nullopt;
  }

  void* pixel_ptr = static_cast<void*>(pixels);
  const auto image_size = beyond::narrow<VkDeviceSize>(tex_width) * tex_height * 4;

  constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

  // allocate temporary buffer for holding texture data to upload
  auto staging_buffer =
      vkh::create_buffer(context, vkh::BufferCreateInfo{.size = image_size,
                                                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
                                                        .debug_name = "Image Staging Buffer"})
          .value();
  BEYOND_DEFER(vkh::destroy_buffer(context, staging_buffer));

  // copy data to buffer
  void* data = context.map(staging_buffer).value();
  memcpy(data, pixel_ptr, beyond::narrow<size_t>(image_size));
  context.unmap(staging_buffer);
  stbi_image_free(pixels);

  const VkExtent3D image_extent = {
      .width = beyond::narrow<uint32_t>(tex_width),
      .height = beyond::narrow<uint32_t>(tex_height),
      .depth = 1,
  };

  vkh::AllocatedImage image =
      vkh::create_image(context,
                        vkh::ImageCreateInfo{
                            .format = image_format,
                            .extent = image_extent,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        })
          .expect("Failed to create image");

  immediate_submit(context, renderer.upload_context(), [&](VkCommandBuffer cmd) {
    static constexpr VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    const VkImageMemoryBarrier image_barrier_to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image.image,
        .subresourceRange = range,
    };

    // barrier the image into the transfer-receive layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &image_barrier_to_transfer);

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
    vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    VkImageMemoryBarrier image_barrier_to_readable = image_barrier_to_transfer;
    image_barrier_to_readable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier_to_readable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_barrier_to_readable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier_to_readable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // barrier the image into the shader readable layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_readable);
  });

  SPDLOG_INFO("Texture loaded {}", filename);
  return image;
}

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
  const charlie::Material* material = nullptr;
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

[[nodiscard]] auto compact_draws(std::span<const charlie::RenderObject> objects)
    -> std::vector<IndirectBatch>
{
  std::vector<IndirectBatch> draws;

  if (objects.empty()) return draws;

  charlie::MeshHandle last_mesh;
  const charlie::Material* last_material = nullptr;

  for (std::uint32_t i = 0; i < objects.size(); ++i) {
    const auto& object = objects[i];
    const bool same_mesh = object.mesh == last_mesh;
    const bool same_material = object.material == last_material;
    if (same_mesh && same_material) {
      BEYOND_ASSERT(!draws.empty());
      ++draws.back().count;
    } else {
      draws.push_back(
          IndirectBatch{.mesh = object.mesh, .material = object.material, .first = i, .count = 1});
      last_material = object.material;
      last_mesh = object.mesh;
    }
  }
  return draws;
}

} // anonymous namespace

namespace charlie {

Renderer::Renderer(Window& window)
    : window_{&window}, resolution_{window.resolution()}, context_{window},
      graphics_queue_{context_.graphics_queue()},
      swapchain_{context_, {.extent = to_extent2d(resolution_)}}
{
  init_depth_image();
  init_frame_data();
  init_descriptors();
  init_pipelines();
  upload_context_ = init_upload_context(context_).expect("Successfully create upload context");

  imgui_render_pass_ =
      std::make_unique<ImguiRenderPass>(*this, window.raw_window(), swapchain_.image_format());

  init_texture();
}

void Renderer::init_frame_data()
{
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index()};

  for (unsigned int i = 0; i < frame_overlap; ++i) {
    FrameData& frame = frames_[i];
    VK_CHECK(
        vkCreateCommandPool(context_, &command_pool_create_info, nullptr, &frame.command_pool));
    frame.main_command_buffer =
        vkh::allocate_command_buffer(
            context_,
            {
                .command_pool = frame.command_pool,
                .debug_name = fmt::format("Main Command Buffer {}", i).c_str(),
            })
            .value();
    frame.present_semaphore =
        vkh::create_semaphore(context_,
                              {.debug_name = fmt::format("Present Semaphore {}", i).c_str()})
            .value();
    frame.render_semaphore =
        vkh::create_semaphore(context_,
                              {.debug_name = fmt::format("Render Semaphore {}", i).c_str()})
            .value();
    frame.render_fence =
        vkh::create_fence(context_, {.flags = VK_FENCE_CREATE_SIGNALED_BIT,
                                     .debug_name = fmt::format("Render Fence {}", i).c_str()})
            .value();

    frame.tracy_vk_ctx = TracyVkContext(context_.physical_device(), context_.device(),
                                        context_.graphics_queue(), frame.main_command_buffer);
  }
}

void Renderer::init_depth_image()
{
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
  descriptor_allocator_ = std::make_unique<vkh::DescriptorAllocator>(context_);
  descriptor_layout_cache_ = std::make_unique<vkh::DescriptorLayoutCache>(context_.device());

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

  static constexpr VkDescriptorSetLayoutBinding bindings[] = {cam_buffer_binding,
                                                              scene_buffer_binding};

  const VkDescriptorSetLayoutCreateInfo global_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = beyond::size(bindings),
      .pBindings = bindings};

  global_descriptor_set_layout_ =
      descriptor_layout_cache_->create_descriptor_layout(global_layout_create_info);

  static constexpr VkDescriptorSetLayoutBinding object_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

  static constexpr VkDescriptorSetLayoutCreateInfo object_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &object_buffer_binding};

  object_descriptor_set_layout_ =
      descriptor_layout_cache_->create_descriptor_layout(object_layout_create_info);

  static constexpr VkDescriptorSetLayoutBinding single_texture_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  static constexpr VkDescriptorSetLayoutCreateInfo single_texture_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &single_texture_binding};

  single_texture_set_layout_ =
      descriptor_layout_cache_->create_descriptor_layout(single_texture_layout_create_info);

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
    frame.camera_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(GPUCameraData),
                               .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Camera Buffer {}", i).c_str(),
                           })
            .value();

    frame.object_buffer =
        vkh::create_buffer(context_,
                           vkh::BufferCreateInfo{
                               .size = sizeof(GPUObjectData) * max_object_count,
                               .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                               .debug_name = fmt::format("Objects Buffer {}", i).c_str(),
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
                               .debug_name = fmt::format("Indirect Buffer {}", i).c_str(),
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

    VkWriteDescriptorSet set_writes[] = {camera_write, scene_write, object_write};

    vkUpdateDescriptorSets(context_, beyond::size(set_writes), set_writes, 0, nullptr);
  }
}

void Renderer::init_pipelines()
{
  shader_compiler_ = std::make_unique<ShaderCompiler>();

  const VkDescriptorSetLayout set_layouts[] = {
      global_descriptor_set_layout_, object_descriptor_set_layout_, single_texture_set_layout_};

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = beyond::size(set_layouts),
      .pSetLayouts = set_layouts,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr,
                                  &mesh_pipeline_layout_));

  const auto vertex_shader_buffer =
      shader_compiler_->compile_shader("mesh.vert.glsl", ShaderStage::vertex).value();
  auto triangle_vert_shader =
      vkh::load_shader_module(context_, vertex_shader_buffer, {.debug_name = "Mesh Vertex Shader"})
          .value();

  const auto fragment_shader_buffer =
      shader_compiler_->compile_shader("mesh.frag.glsl", ShaderStage::fragment).value();
  auto triangle_frag_shader = vkh::load_shader_module(context_, fragment_shader_buffer,
                                                      {.debug_name = "Mesh Fragment Shader"})
                                  .value();
  BEYOND_DEFER({
    vkDestroyShaderModule(context_, triangle_vert_shader, nullptr);
    vkDestroyShaderModule(context_, triangle_frag_shader, nullptr);
  });

  const VkPipelineShaderStageCreateInfo triangle_shader_stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = triangle_vert_shader,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = triangle_frag_shader,
       .pName = "main"}};

  static constexpr VkVertexInputBindingDescription binding_descriptions[] = {
      {
          .binding = 0,
          .stride = sizeof(beyond::Vec3),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
      {
          .binding = 1,
          .stride = sizeof(beyond::Vec3),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      },
      {
          .binding = 2,
          .stride = sizeof(beyond::Vec2),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      }};

  static constexpr VkVertexInputAttributeDescription attribute_descriptions[] = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 1, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 2, .binding = 2, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0}};

  const VkFormat color_attachment_formats[] = {swapchain_.image_format()};
  mesh_pipeline_ =
      vkh::create_graphics_pipeline(
          context_,
          {.layout = mesh_pipeline_layout_,
           .pipeline_rendering_create_info =
               vkh::PipelineRenderingCreateInfo{
                   .color_attachment_formats = color_attachment_formats,
                   .depth_attachment_format = depth_format_,
               },
           .debug_name = "Mesh Graphics Pipeline",
           .vertex_input_state_create_info = {.binding_descriptions = binding_descriptions,
                                              .attribute_descriptions = attribute_descriptions},
           .shader_stages = triangle_shader_stages,
           .cull_mode = vkh::CullMode::none})
          .value();

  create_material(mesh_pipeline_, mesh_pipeline_layout_, "default");
}

void Renderer::init_texture()
{
  texture_.image =
      load_image_from_file(*this, "../../assets/models/lost_empire/lost_empire-RGBA.png").value();
  texture_.image_view =
      vkh::create_image_view(
          context_,
          {.image = texture_.image.image,
           .format = VK_FORMAT_R8G8B8A8_SRGB,
           .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT}})
          .expect("Failed to create image view");

  // Create sampler
  const VkSamplerCreateInfo sampler_info =
      sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  vkCreateSampler(context_, &sampler_info, nullptr, &blocky_sampler_);

  // alloc descriptor set for material
  auto* material = get_material("default");
  BEYOND_ENSURE(material != nullptr);
  {
    material->texture_set = descriptor_allocator_->allocate(single_texture_set_layout_).value();

    // write to the descriptor set so that it points to our empire_diffuse texture
    const VkDescriptorImageInfo image_buffer_info = {
        .sampler = blocky_sampler_,
        .imageView = texture_.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkWriteDescriptorSet texture1 = write_descriptor_image(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, material->texture_set, &image_buffer_info, 0);
    vkUpdateDescriptorSets(context_, 1, &texture1, 0, nullptr);
  }
}

void Renderer::render(const charlie::Camera& camera)
{
  ZoneScopedN("Render");

  const auto& frame = current_frame();
  constexpr std::uint64_t one_second = 1'000'000'000;

  std::uint32_t swapchain_image_index = 0;
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

  [[maybe_unused]] const auto current_swapchain_image = swapchain_.images()[swapchain_image_index];
  const auto current_swapchain_image_view = swapchain_.image_views()[swapchain_image_index];

  VK_CHECK(vkResetCommandBuffer(frame.main_command_buffer, 0));

  VkCommandBuffer cmd = frame.main_command_buffer;

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  imgui_render_pass_->pre_render();

  {
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    {
      TracyVkCollect(frame.tracy_vk_ctx, cmd);
      TracyVkZone(frame.tracy_vk_ctx, cmd, "Render");

      transit_current_swapchain_image_for_rendering(cmd, current_swapchain_image);

      {
        ZoneScopedN("Mesh Render Pass");
        TracyVkZone(frame.tracy_vk_ctx, cmd, "Mesh Render Pass");

        const VkRenderingAttachmentInfo color_attachments_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = current_swapchain_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = VkClearValue{.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}},
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

        draw_scene(cmd, camera);
        vkCmdEndRendering(cmd);
      }

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
void Renderer::present(uint32_t& swapchain_image_index)
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

[[nodiscard]] auto Renderer::create_material(VkPipeline pipeline, VkPipelineLayout layout,
                                             std::string name) -> Material&
{
  auto [itr, inserted] = materials_.try_emplace(std::move(name), Material{pipeline, layout});
  BEYOND_ENSURE(inserted);
  return itr->second;
}

[[nodiscard]] auto Renderer::get_material(const std::string& name) -> Material*
{
  auto it = materials_.find(name);
  return it == materials_.end() ? nullptr : &it->second;
}

[[nodiscard]] auto Renderer::upload_mesh_data(const CPUMesh& cpu_mesh) -> MeshHandle
{
  static constexpr auto buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  const vkh::AllocatedBuffer position_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.positions, buffer_usage,
                    fmt::format("{} Position", cpu_mesh.name).c_str())
          .value();
  const vkh::AllocatedBuffer normal_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.normals, buffer_usage,
                    fmt::format("{} Normal", cpu_mesh.name).c_str())
          .value();
  const vkh::AllocatedBuffer uv_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.uv, buffer_usage,
                    fmt::format("{} Texcoord", cpu_mesh.name).c_str())
          .value();

  const vkh::AllocatedBuffer index_buffer =
      upload_buffer(context_, upload_context_, cpu_mesh.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    fmt::format("{} Index", cpu_mesh.name).c_str())
          .value();

  return meshes_.insert(
      Mesh{.position_buffer = position_buffer,
           .normal_buffer = normal_buffer,
           .uv_buffer = uv_buffer,
           .index_buffer = index_buffer,
           .vertices_count = beyond::narrow<std::uint32_t>(cpu_mesh.positions.size()),
           .index_count = beyond::narrow<std::uint32_t>(cpu_mesh.indices.size())});
}

void Renderer::draw_scene(VkCommandBuffer cmd, const charlie::Camera& camera)
{

  // Camera

  const beyond::Mat4 view = camera.view_matrix();
  const beyond::Mat4 projection = camera.proj_matrix();

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
  char* scene_data;
  vmaMapMemory(context_.allocator(), scene_parameter_buffer_.allocation, (void**)&scene_data);

  const size_t frame_index = frame_number_ % frame_overlap;

  scene_data += context_.align_uniform_buffer_size(sizeof(GPUSceneParameters)) * frame_index;
  memcpy(scene_data, &scene_parameters_, sizeof(GPUSceneParameters));
  vmaUnmapMemory(context_.allocator(), scene_parameter_buffer_.allocation);

  // Render objects

  const Material* last_material = nullptr;
  beyond::optional<MeshHandle> last_mesh;

  render_objects_.clear();
  for (const auto [node_index, mesh] : scene_->meshe_components_) {
    render_objects_.push_back(RenderObject{
        .mesh = mesh,
        .material = get_material("default"),
        .model_matrix = scene_->global_transforms[node_index],
    });
  }

  // Copy to object buffer
  auto* object_data =
      beyond::narrow<GPUObjectData*>(context_.map(current_frame().object_buffer).value());

  const std::size_t object_count = render_objects_.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (std::size_t i = 0; i < scene_->global_transforms.size(); ++i) {
    object_data[i].model = scene_->global_transforms[i];
  }

  context_.unmap(current_frame().object_buffer);

  // Generate draws
  const auto draws = compact_draws(render_objects_);

  {
    auto* draw_commands =
        context_.map<VkDrawIndexedIndirectCommand>(current_frame().indirect_buffer).value();
    BEYOND_ENSURE(draw_commands != nullptr);

    for (std::uint32_t i = 0; i < object_count; ++i) {
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

  for (const IndirectBatch& draw : draws) {
    if (draw.material != last_material) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline);

      const uint32_t uniform_offset =
          beyond::narrow<uint32_t>(context_.align_uniform_buffer_size(sizeof(GPUSceneParameters))) *
          beyond::narrow<uint32_t>(frame_index);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline_layout,
                              0, 1, &current_frame().global_descriptor_set, 1, &uniform_offset);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline_layout,
                              1, 1, &current_frame().object_descriptor_set, 0, nullptr);
      if (draw.material->texture_set != VK_NULL_HANDLE) {
        // texture descriptor
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                draw.material->pipeline_layout, 2, 1, &draw.material->texture_set,
                                0, nullptr);
      }
    }

    if (draw.mesh != last_mesh) {
      const auto& mesh = meshes_.try_get(draw.mesh).expect("Cannot find mesh by handle!");

      constexpr VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.position_buffer.buffer, &offset);
      vkCmdBindVertexBuffers(cmd, 1, 1, &mesh.normal_buffer.buffer, &offset);
      vkCmdBindVertexBuffers(cmd, 2, 1, &mesh.uv_buffer.buffer, &offset);
      vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    const VkDeviceSize indirect_offset = draw.first * sizeof(VkDrawIndexedIndirectCommand);
    constexpr uint32_t draw_stride = sizeof(VkDrawIndexedIndirectCommand);
    vkCmdDrawIndexedIndirect(cmd, current_frame().indirect_buffer, indirect_offset, draw.count,
                             draw_stride);

    last_material = draw.material;
    last_mesh = draw.mesh;
  }
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  imgui_render_pass_ = nullptr;

  vkDestroyImageView(context_, texture_.image_view, nullptr);
  vkh::destroy_image(context_, texture_.image);
  vkDestroySampler(context_, blocky_sampler_, nullptr);

  for (const auto& mesh : meshes_.values()) { destroy_mesh(context_, mesh); }

  vkDestroyCommandPool(context_, upload_context_.command_pool, nullptr);
  vkDestroyFence(context_, upload_context_.fence, nullptr);

  vkDestroyPipeline(context_, mesh_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_, mesh_pipeline_layout_, nullptr);

  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);

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

auto Renderer::set_scene(std::unique_ptr<const Scene> scene) -> const Scene&
{
  scene_ = std::move(scene);
  return *scene_;
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
  swapchain_ = vkh::Swapchain(context_,
                              vkh::SwapchainCreateInfo{.extent = charlie::to_extent2d(resolution()),
                                                       .old_swapchain = swapchain_.get()});

  // recreate depth image
  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vkh::destroy_image(context_, depth_image_);
  init_depth_image();
}

} // namespace charlie