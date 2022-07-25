#include "renderer.hpp"

#include "vulkan_helpers/commands.hpp"
#include "vulkan_helpers/descriptor_pool.hpp"
#include "vulkan_helpers/descriptor_utils.hpp"
#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/sync.hpp"

#include "tiny_obj_loader.h"

#include <beyond/math/transform.hpp>
#include <cstddef>

#include <stb_image.h>

#include <beyond/types/optional.hpp>

#include "camera.hpp"
#include "mesh.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace {

struct ObjectPushConstants {
  beyond::Mat4 transformation;
};

struct GPUObjectData {
  beyond::Mat4 model;
};

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
};

constexpr std::size_t max_object_count = 10000;

[[nodiscard]] auto sampler_create_info(
    VkFilter filters,
    VkSamplerAddressMode address_mode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
    -> VkSamplerCreateInfo
{
  VkSamplerCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.pNext = nullptr;

  info.magFilter = filters;
  info.minFilter = filters;
  info.addressModeU = address_mode;
  info.addressModeV = address_mode;
  info.addressModeW = address_mode;

  return info;
}
auto write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
                            VkDescriptorImageInfo* image_info, uint32_t binding)
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

[[nodiscard]] constexpr auto to_extent2d(Resolution res)
{
  return VkExtent2D{.width = res.width, .height = res.height};
}

[[nodiscard]] auto init_swapchain(vkh::Context& context, Window& window)
    -> vkh::Swapchain
{
  vkh::Swapchain swapchain;
  swapchain = vkh::Swapchain(
      context, vkh::SwapchainCreateInfo{to_extent2d(window.resolution())});
  return swapchain;
}

[[nodiscard]] auto init_default_render_pass(vkh::Context& context,
                                            const vkh::Swapchain& swapchain,
                                            VkFormat depth_image_format)
    -> VkRenderPass
{
  // the renderpass will use this color attachment.
  const VkAttachmentDescription color_attachment = {
      .format = swapchain.image_format(),
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

  static constexpr VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  const VkAttachmentDescription depth_attachment = {
      .flags = 0,
      .format = depth_image_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  static constexpr VkAttachmentReference depth_attachment_ref = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  static constexpr VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
      .pDepthStencilAttachment = &depth_attachment_ref,
  };

  const VkAttachmentDescription attachments[] = {color_attachment,
                                                 depth_attachment};

  const VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = beyond::size(attachments),
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = &subpass,
  };

  VkRenderPass render_pass{};
  VK_CHECK(vkCreateRenderPass(context.device(), &render_pass_create_info,
                              nullptr, &render_pass));
  return render_pass;
}

[[nodiscard]] auto init_framebuffers(const Window& window,
                                     vkh::Context& context,
                                     const vkh::Swapchain& swapchain,
                                     VkRenderPass render_pass,
                                     VkImageView depth_image_view)
{
  Resolution res = window.resolution();
  VkFramebufferCreateInfo framebuffers_create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .renderPass = render_pass,
      .attachmentCount = 2,
      .width = res.width,
      .height = res.height,
      .layers = 1,
  };

  const auto swapchain_imagecount = swapchain.images().size();

  std::vector<VkFramebuffer> framebuffers(swapchain_imagecount);

  for (std::size_t i = 0; i < swapchain_imagecount; ++i) {
    VkImageView attachments[2] = {swapchain.image_views()[i], depth_image_view};

    framebuffers_create_info.pAttachments = attachments;
    VK_CHECK(vkCreateFramebuffer(context.device(), &framebuffers_create_info,
                                 nullptr, &framebuffers[i]));
  }
  return framebuffers;
}

[[nodiscard]] auto load_image_from_file(charlie::Renderer& renderer,
                                        const char* filename)
    -> beyond::optional<vkh::Image>
{
  vkh::Context& context = renderer.context();

  int tex_width{}, tex_height{}, tex_channels{};
  stbi_uc* pixels = stbi_load(filename, &tex_width, &tex_height, &tex_channels,
                              STBI_rgb_alpha);

  if (!pixels) {
    fmt::print(stderr, "Failed to load texture file {}\n", filename);
    std::fflush(stderr);
    return beyond::nullopt;
  }

  void* pixel_ptr = static_cast<void*>(pixels);
  const auto image_size = static_cast<VkDeviceSize>(tex_width * tex_height * 4);

  constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

  // allocate temporary buffer for holding texture data to upload
  auto staging_buffer =
      vkh::create_buffer(
          context,
          vkh::BufferCreateInfo{.size = image_size,
                                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
                                .debug_name = "Image Staging Buffer"})
          .value();

  // copy data to buffer
  void* data = context.map(staging_buffer).value();
  memcpy(data, pixel_ptr, static_cast<size_t>(image_size));
  context.unmap(staging_buffer);
  stbi_image_free(pixels);

  VkExtent3D image_extent = {
      .width = static_cast<uint32_t>(tex_width),
      .height = static_cast<uint32_t>(tex_height),
      .depth = 1,
  };

  const VkImageCreateInfo image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = image_format,
      .extent = image_extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

  vkh::Image image;
  VmaAllocationCreateInfo image_allocation_create_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY};

  // allocate and create the image
  VK_CHECK(vmaCreateImage(context.allocator(), &image_create_info,
                          &image_allocation_create_info, &image.image,
                          &image.allocation, nullptr));

  renderer.immediate_submit([&](VkCommandBuffer cmd) {
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
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_barrier_to_transfer);

    const VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
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
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    VkImageMemoryBarrier image_barrier_to_readable = image_barrier_to_transfer;

    image_barrier_to_readable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier_to_readable.newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_barrier_to_readable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier_to_readable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // barrier the image into the shader readable layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &image_barrier_to_readable);
  });

  vmaDestroyBuffer(context.allocator(), staging_buffer.buffer,
                   staging_buffer.allocation);
  fmt::print("Texture loaded successfully {}\n", filename);
  return image;
}

} // anonymous namespace

namespace charlie {

Renderer::Renderer(Window& window)
    : window_{&window}, context_{window},
      graphics_queue_{context_.graphics_queue()},
      graphics_queue_family_index{context_.graphics_queue_family_index()},
      swapchain_{init_swapchain(context_, window)}
{
  init_depth_image();

  render_pass_ = init_default_render_pass(context_, swapchain_, depth_format_);
  framebuffers_ = init_framebuffers(window, context_, swapchain_, render_pass_,
                                    depth_image_view_),

  init_frame_data();
  init_descriptors();
  init_pipelines();
  init_upload_context();

  init_imgui();

  texture_.image =
      load_image_from_file(*this, "assets/lost_empire/lost_empire-RGBA.png")
          .value();
  const VkImageViewCreateInfo image_view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = texture_.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_SRGB,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};
  VK_CHECK(vkCreateImageView(context_, &image_view_create_info, nullptr,
                             &texture_.image_view));

  meshes_["lost_empire"] =
      load_mesh(context_, "assets/lost_empire/lost_empire.obj");

  // Create sampler
  const VkSamplerCreateInfo sampler_info =
      sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  vkCreateSampler(context_, &sampler_info, nullptr, &blocky_sampler_);

  // alloc descriptor set for material
  auto* material = get_material("default");
  BEYOND_ENSURE(material != nullptr);
  {
    material->texture_set =
        descriptor_allocator_->allocate(single_texture_set_layout_).value();

    // write to the descriptor set so that it points to our empire_diffuse
    // texture
    VkDescriptorImageInfo image_buffer_info = {
        .sampler = blocky_sampler_,
        .imageView = texture_.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet texture1 =
        write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               material->texture_set, &image_buffer_info, 0);
    vkUpdateDescriptorSets(context_, 1, &texture1, 0, nullptr);
  }
} // namespace charlie

void Renderer::init_frame_data()
{
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index()};

  for (auto& frame : frames_) {
    VK_CHECK(vkCreateCommandPool(context_, &command_pool_create_info, nullptr,
                                 &frame.command_pool));
    frame.main_command_buffer =
        vkh::allocate_command_buffer(context_,
                                     {
                                         .command_pool = frame.command_pool,
                                         .debug_name = "Main Command Buffer",
                                     })
            .value();
    frame.present_semaphore =
        vkh::create_semaphore(context_, {.debug_name = "Present Semaphore"})
            .value();
    frame.render_semaphore =
        vkh::create_semaphore(context_, {.debug_name = "Render Semaphore"})
            .value();
    frame.render_fence =
        vkh::create_fence(context_, {.flags = VK_FENCE_CREATE_SIGNALED_BIT,
                                     .debug_name = "Render Fence"})
            .value();
  }
}

void Renderer::init_depth_image()
{
  depth_format_ = VK_FORMAT_D32_SFLOAT;

  const VkImageCreateInfo image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depth_format_,
      .extent = VkExtent3D{window_->resolution().width,
                           window_->resolution().height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

  static constexpr VmaAllocationCreateInfo depth_image_alloc_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags =
          VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VK_CHECK(vmaCreateImage(context_.allocator(), &image_create_info,
                          &depth_image_alloc_info, &depth_image_.image,
                          &depth_image_.allocation, nullptr));

  const VkImageViewCreateInfo image_view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = depth_image_.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_format_,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};

  VK_CHECK(vkCreateImageView(context_, &image_view_create_info, nullptr,
                             &depth_image_view_));
}

void Renderer::init_descriptors()
{
  descriptor_allocator_ = std::make_unique<vkh::DescriptorAllocator>(context_);
  descriptor_layout_cache_ =
      std::make_unique<vkh::DescriptorLayoutCache>(context_.device());

  static constexpr VkDescriptorSetLayoutBinding cam_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

  const VkDescriptorSetLayoutCreateInfo global_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &cam_buffer_binding};

  global_descriptor_set_layout_ =
      descriptor_layout_cache_->create_descriptor_layout(
          global_layout_create_info);

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
      descriptor_layout_cache_->create_descriptor_layout(
          object_layout_create_info);

  static constexpr VkDescriptorSetLayoutBinding single_texture_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  static constexpr VkDescriptorSetLayoutCreateInfo
      single_texture_layout_create_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .bindingCount = 1,
          .pBindings = &single_texture_binding};

  single_texture_set_layout_ =
      descriptor_layout_cache_->create_descriptor_layout(
          single_texture_layout_create_info);

  for (auto i = 0u; i < frame_overlap; ++i) {
    FrameData& frame = frames_[i];
    frame.camera_buffer =
        vkh::create_buffer(
            context_,
            vkh::BufferCreateInfo{
                .size = sizeof(GPUCameraData),
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .debug_name = fmt::format("Camera Buffer {}", i).c_str(),
            })
            .value();

    frame.object_buffer =
        vkh::create_buffer(
            context_,
            vkh::BufferCreateInfo{
                .size = sizeof(GPUObjectData) * max_object_count,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .debug_name = fmt::format("Objects Buffer {}", i).c_str(),
            })
            .value();

    frame.global_descriptor_set =
        descriptor_allocator_->allocate(global_descriptor_set_layout_).value();
    frame.object_descriptor_set =
        descriptor_allocator_->allocate(object_descriptor_set_layout_).value();

    const VkDescriptorBufferInfo camera_buffer_info = {
        .buffer = frame.camera_buffer.buffer,
        .offset = 0,
        .range = sizeof(GPUCameraData)};

    const VkWriteDescriptorSet camera_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.global_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &camera_buffer_info};

    const VkDescriptorBufferInfo object_buffer_info = {
        .buffer = frame.object_buffer.buffer,
        .offset = 0,
        .range = sizeof(GPUObjectData) * max_object_count};

    const VkWriteDescriptorSet object_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.object_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &object_buffer_info};

    VkWriteDescriptorSet set_writes[] = {camera_write, object_write};

    vkUpdateDescriptorSets(context_, beyond::size(set_writes), set_writes, 0,
                           nullptr);
  }
}

void Renderer::init_pipelines()
{
  static constexpr VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(ObjectPushConstants)};

  const VkDescriptorSetLayout set_layouts[] = {global_descriptor_set_layout_,
                                               object_descriptor_set_layout_,
                                               single_texture_set_layout_};

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = beyond::size(set_layouts),
      .pSetLayouts = set_layouts,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info,
                                  nullptr, &mesh_pipeline_layout_));

  auto triangle_vert_shader =
      vkh::load_shader_module_from_file(context_, "shaders/triangle.vert.spv",
                                        {.debug_name = "Mesh Vertex Shader"})
          .value();
  auto triangle_frag_shader =
      vkh::load_shader_module_from_file(context_, "shaders/triangle.frag.spv",
                                        {.debug_name = "Mesh Fragment Shader"})
          .value();

  const VkPipelineShaderStageCreateInfo triangle_shader_stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = triangle_vert_shader,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = triangle_frag_shader,
       .pName = "main"}};

  VkVertexInputBindingDescription binding_descriptions[] = {
      Vertex::binding_description()};
  default_pipeline_ =
      vkh::create_graphics_pipeline(
          context_,
          {.layout = mesh_pipeline_layout_,
           .render_pass = render_pass_,
           .window_extend = to_extent2d(window_->resolution()),
           .debug_name = "Mesh Graphics Pipeline",
           .vertex_input_state_create_info =
               {.binding_descriptions = binding_descriptions,
                .attribute_descriptions = Vertex::attributes_descriptions()},
           .shader_stages = triangle_shader_stages,
           .cull_mode = vkh::CullMode::none})
          .value();

  vkDestroyShaderModule(context_, triangle_vert_shader, nullptr);
  vkDestroyShaderModule(context_, triangle_frag_shader, nullptr);

  create_material(default_pipeline_, mesh_pipeline_layout_, "default");
}

void Renderer::init_upload_context()
{
  upload_context_.fence =
      vkh::create_fence(context_,
                        vkh::FenceCreateInfo{.debug_name = "Upload Fence"})
          .value();
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context_.graphics_queue_family_index()};

  VK_CHECK(vkCreateCommandPool(context_, &command_pool_create_info, nullptr,
                               &upload_context_.command_pool));
}

void Renderer::init_imgui()
{
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  static constexpr VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  static constexpr VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000,
      .poolSizeCount = beyond::size(pool_sizes),
      .pPoolSizes = pool_sizes,
  };

  VK_CHECK(vkCreateDescriptorPool(context_.device(), &pool_info, nullptr,
                                  &imgui_pool_));

  // 2: initialize imgui library

  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForVulkan(window_->glfw_window(), true);

  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = context_.instance(),
      .PhysicalDevice = context_.physical_device(),
      .Device = context_.device(),
      .Queue = context_.graphics_queue(),
      .DescriptorPool = imgui_pool_,
      .MinImageCount = 3,
      .ImageCount = 3,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
  };
  ImGui_ImplVulkan_Init(&init_info, render_pass_);

  // execute a gpu command to upload imgui font textures
  immediate_submit(
      [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

  // clear font textures from cpu data
  ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Renderer::render(const charlie::Camera& camera)
{
  const auto& frame = current_frame();
  constexpr std::uint64_t one_second = 1'000'000'000;

  // wait until the GPU has finished rendering the last frame.
  VK_CHECK(vkWaitForFences(context_, 1, &frame.render_fence, true, one_second));
  VK_CHECK(vkResetFences(context_, 1, &frame.render_fence));

  std::uint32_t swapchain_image_index = 0;
  VK_CHECK(vkAcquireNextImageKHR(context_, swapchain_, one_second,
                                 frame.present_semaphore, nullptr,
                                 &swapchain_image_index));

  VK_CHECK(vkResetCommandBuffer(frame.main_command_buffer, 0));

  VkCommandBuffer cmd = frame.main_command_buffer;
  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  static constexpr VkClearValue clear_values[] = {
      {.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}},
      {.depthStencil = {.depth = 1.f}}};

  ImGui::Render();
  const VkRenderPassBeginInfo rp_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = render_pass_,
      .framebuffer = framebuffers_[swapchain_image_index],
      .renderArea =
          {
              .offset =
                  {
                      .x = 0,
                      .y = 0,
                  },
              .extent = to_extent2d(window_->resolution()),
          },
      .clearValueCount = beyond::size(clear_values),
      .pClearValues = clear_values,
  };

  vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

  draw_objects(cmd, render_objects_, camera);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  static constexpr VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  const VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame.present_semaphore,
      .pWaitDstStageMask = &waitStage,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &frame.render_semaphore,
  };

  VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit, frame.render_fence));

  VkSwapchainKHR swapchain = swapchain_.get();
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame.render_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &swapchain_image_index,
  };
  VK_CHECK(vkQueuePresentKHR(graphics_queue_, &present_info));

  ++frame_number_;
}

[[nodiscard]] auto Renderer::create_material(VkPipeline pipeline,
                                             VkPipelineLayout layout,
                                             std::string name) -> Material&
{
  [[maybe_unused]] auto [itr, inserted] =
      materials_.try_emplace(std::move(name), Material{pipeline, layout});
  BEYOND_ENSURE(inserted);
  return itr->second;
}

[[nodiscard]] auto Renderer::get_material(const std::string& name) -> Material*
{
  auto it = materials_.find(name);
  return it == materials_.end() ? nullptr : &it->second;
}

[[nodiscard]] auto Renderer::get_mesh(const std::string& name) -> Mesh*
{
  auto it = meshes_.find(name);
  return it == meshes_.end() ? nullptr : &it->second;
}

[[nodiscard]] auto Renderer::load_mesh(vkh::Context& context,
                                       const char* filename) -> Mesh
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  if (const bool ret =
          tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename);
      !ret) {
    beyond::panic(fmt::format("Mesh loading error: {}", err));
  }

  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  for (const auto& shape : shapes) {
    std::size_t index_offset = 0;
    for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      const auto fv = std::size_t(shape.mesh.num_face_vertices[f]);
      for (std::size_t v = 0; v < fv; v++) {
        const tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        const auto vx = attrib.vertices[3 * idx.vertex_index + 0];
        const auto vy = attrib.vertices[3 * idx.vertex_index + 1];
        const auto vz = attrib.vertices[3 * idx.vertex_index + 2];

        const auto nx = attrib.normals[3 * idx.normal_index + 0];
        const auto ny = attrib.normals[3 * idx.normal_index + 1];
        const auto nz = attrib.normals[3 * idx.normal_index + 2];

        // vertex uv
        const auto ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        const auto uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        vertices.push_back(Vertex{.position = {vx, vy, vz},
                                  .normal = {nx, ny, nz},
                                  .uv = {ux, 1 - uy}});
      }
      index_offset += fv;
    }
  }

  return upload_mesh_data(context, vertices, indices);
}

[[nodiscard]] auto
Renderer::upload_mesh_data(vkh::Context& /*context*/,
                           std::span<const Vertex> vertices,
                           std::span<const std::uint32_t> indices) -> Mesh
{

  // const auto index_buffer_size = indices.size() * sizeof(uint32_t);

  vkh::Buffer vertex_buffer =
      upload_buffer(vertices.size_bytes(), vertices.data(),
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
          .value();

  //  auto index_staging_buffer =
  //      vkh::create_buffer_from_data(context,
  //                                   {.size = index_buffer_size,
  //                                    .usage =
  //                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  //                                    .memory_usage =
  //                                    VMA_MEMORY_USAGE_CPU_TO_GPU, .debug_name
  //                                    = "Mesh Index Staging Buffer"},
  //                                   indices.data())
  //          .value();
  //
  //  auto index_buffer =
  //      vkh::create_buffer(context, {.size = index_buffer_size,
  //                                   .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
  //                                   |
  //                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  //                                   .memory_usage =
  //                                   VMA_MEMORY_USAGE_GPU_ONLY, .debug_name =
  //                                   "Mesh Index Buffer"})
  //          .value();
  //
  //  immediate_submit([=](VkCommandBuffer cmd) {
  //    const VkBufferCopy copy = {
  //        .srcOffset = 0,
  //        .dstOffset = 0,
  //        .size = index_buffer_size,
  //    };
  //    vkCmdCopyBuffer(cmd, index_staging_buffer.buffer, index_buffer.buffer,
  //    1,
  //                    &copy);
  //  });

  // vkh::destroy_buffer(context, index_staging_buffer);

  return Mesh{.vertex_buffer = vertex_buffer,
              //.index_buffer = index_buffer,
              .vertices_count = static_cast<std::uint32_t>(vertices.size()),
              .index_count = static_cast<std::uint32_t>(indices.size())};
}

auto Renderer::upload_buffer(std::size_t size, const void* data,
                             VkBufferUsageFlags usage)
    -> vkh::Expected<vkh::Buffer>
{
  return vkh::create_buffer(context_,
                            {.size = size,
                             .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                             .debug_name = "Mesh Vertex Buffer"})
      .and_then([=, this](vkh::Buffer gpu_buffer) {
        auto vertex_staging_buffer =
            vkh::create_buffer_from_data(
                context_,
                {.size = size,
                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                 .debug_name = "Mesh Vertex Staging Buffer"},
                data)
                .value();
        immediate_submit([=](VkCommandBuffer cmd) {
          const VkBufferCopy copy = {
              .srcOffset = 0,
              .dstOffset = 0,
              .size = size,
          };
          vkCmdCopyBuffer(cmd, vertex_staging_buffer.buffer, gpu_buffer.buffer,
                          1, &copy);
        });
        vkh::destroy_buffer(context_, vertex_staging_buffer);
        return vkh::Expected<vkh::Buffer>(gpu_buffer);
      });
}

void Renderer::draw_objects(VkCommandBuffer cmd,
                            std::span<RenderObject> objects,
                            const charlie::Camera& camera)
{
  // Copy to object buffer
  void* object_data = nullptr;
  vmaMapMemory(context_.allocator(), current_frame().object_buffer.allocation,
               &object_data);

  auto* object_data_typed = static_cast<GPUObjectData*>(object_data);

  std::size_t object_count = objects.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (std::size_t i = 0; i < objects.size(); ++i) {
    object_data_typed[i].model = objects[i].model_matrix;
  }

  vmaUnmapMemory(context_.allocator(),
                 current_frame().object_buffer.allocation);

  // Camera

  //  const auto res = window_->resolution();
  //  const float aspect =
  //      static_cast<float>(res.width) / static_cast<float>(res.height);

  const beyond::Mat4 view = camera.view_matrix();
  const beyond::Mat4 projection = camera.proj_matrix();

  const GPUCameraData cam_data = {
      .view = view,
      .proj = projection,
      .view_proj = projection * view,
  };

  vkh::Buffer& camera_buffer = current_frame().camera_buffer;

  void* data = nullptr;
  vmaMapMemory(context_.allocator(), camera_buffer.allocation, &data);
  memcpy(data, &cam_data, sizeof(GPUCameraData));
  vmaUnmapMemory(context_.allocator(), camera_buffer.allocation);

  // Render objects

  const Material* last_material = nullptr;
  const Mesh* last_mesh = nullptr;

  for (std::size_t i = 0; i < object_count; ++i) {
    RenderObject& object = objects[i];
    if (object.material != last_material) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              object.material->pipeline_layout, 0, 1,
                              &current_frame().global_descriptor_set, 0,
                              nullptr);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              object.material->pipeline_layout, 1, 1,
                              &current_frame().object_descriptor_set, 0,
                              nullptr);
      if (object.material->texture_set != VK_NULL_HANDLE) {
        // texture descriptor
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                object.material->pipeline_layout, 2, 1,
                                &object.material->texture_set, 0, nullptr);
      }
    }

    if (object.mesh != last_mesh) {
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertex_buffer.buffer,
                             &offset);
      //    vkCmdBindIndexBuffer(cmd, object.mesh->index_buffer.buffer, 0,
      //                         VK_INDEX_TYPE_UINT32);
    }

    const ObjectPushConstants constants = {.transformation =
                                               object.model_matrix};
    vkCmdPushConstants(cmd, object.material->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(ObjectPushConstants), &constants);
    vkCmdDraw(cmd, object.mesh->vertices_count, 1, 0, 0);

    last_material = object.material;
    last_mesh = object.mesh;

    //    vkCmdDrawIndexed(cmd, object.mesh->index_count, 1, 0, 0,
    //                     static_cast<std::uint32_t>(i));
  }
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  vkDestroyDescriptorPool(context_, imgui_pool_, nullptr);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyImageView(context_, texture_.image_view, nullptr);
  vmaDestroyImage(context_.allocator(), texture_.image.image,
                  texture_.image.allocation);

  for (const auto& [_, mesh] : meshes_) {
    vkh::destroy_buffer(context_, mesh.vertex_buffer);
    vkh::destroy_buffer(context_, mesh.index_buffer);
  }

  vkDestroyCommandPool(context_, upload_context_.command_pool, nullptr);
  vkDestroyFence(context_, upload_context_.fence, nullptr);

  vkDestroyPipeline(context_, default_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_, mesh_pipeline_layout_, nullptr);

  vkDestroyImageView(context_, depth_image_view_, nullptr);
  vmaDestroyImage(context_.allocator(), depth_image_.image,
                  depth_image_.allocation);

  vkDestroySampler(context_, blocky_sampler_, nullptr);

  descriptor_allocator_ = nullptr;
  descriptor_layout_cache_ = nullptr;

  for (auto& frame : frames_) {
    vkh::destroy_buffer(context_, frame.object_buffer);
    vkh::destroy_buffer(context_, frame.camera_buffer);

    vkDestroyFence(context_, frame.render_fence, nullptr);
    vkDestroySemaphore(context_, frame.render_semaphore, nullptr);
    vkDestroySemaphore(context_, frame.present_semaphore, nullptr);
    vkDestroyCommandPool(context_, frame.command_pool, nullptr);
  }

  for (auto framebuffer : framebuffers_) {
    vkDestroyFramebuffer(context_, framebuffer, nullptr);
  }
  vkDestroyRenderPass(context_, render_pass_, nullptr);
}
void Renderer::add_object(RenderObject object)
{
  render_objects_.push_back(object);
}

void Renderer::immediate_submit(
    beyond::function_ref<void(VkCommandBuffer)> function)
{
  VkCommandBuffer cmd =
      vkh::allocate_command_buffer(
          context_, {.command_pool = upload_context_.command_pool,
                     .debug_name = "Uploading Command Buffer"})
          .value();

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  const VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1,
                               .pCommandBuffers = &cmd};

  VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit, upload_context_.fence));
  VK_CHECK(
      vkWaitForFences(context_, 1, &upload_context_.fence, true, 9999999999));
  VK_CHECK(vkResetFences(context_, 1, &upload_context_.fence));
  VK_CHECK(vkResetCommandPool(context_, upload_context_.command_pool, 0));
}

} // namespace charlie