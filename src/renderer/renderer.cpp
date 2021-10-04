#include "renderer.hpp"

#include "vulkan_helpers/commands.hpp"
#include "vulkan_helpers/descriptor_pool.hpp"
#include "vulkan_helpers/error_handling.hpp"
#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/sync.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <beyond/math/transform.hpp>

#include "mesh.hpp"

struct MeshPushConstants {
  beyond::Mat4 transformation;
};

namespace {

constexpr std::size_t max_object_count = 10000;

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

  meshes_["bunny"] = load_mesh(context_, "bunny.obj");

  static constexpr Vertex triangle_vertices[3] = {
      {.position = {1.f, 1.f, 0.5f}, .color = {0.f, 1.f, 0.0f}},
      {.position = {-1.f, 1.f, 0.5f}, .color = {0.f, 1.f, 0.0f}},
      {.position = {0.f, -1.f, 0.5f}, .color = {0.f, 1.f, 0.0f}}};
  static constexpr std::uint32_t triangle_indices[3] = {0, 1, 2};
  meshes_["triangle"] =
      upload_mesh_data(context_, triangle_vertices, triangle_indices);
}

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
      .pNext = nullptr,
      .flags = 0,
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
  static constexpr VkDescriptorSetLayoutBinding cam_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

  const VkDescriptorSetLayoutCreateInfo global_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &cam_buffer_binding};
  VK_CHECK(vkCreateDescriptorSetLayout(context_, &global_layout_create_info,
                                       nullptr,
                                       &global_descriptor_set_layout_));

  static constexpr VkDescriptorSetLayoutBinding object_buffer_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};

  const VkDescriptorSetLayoutCreateInfo object_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &object_buffer_binding};

  VK_CHECK(vkCreateDescriptorSetLayout(context_, &object_layout_create_info,
                                       nullptr,
                                       &object_descriptor_set_layout_));

  std::array sizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10}};

  descriptor_pool_ =
      vkh::create_descriptor_pool(context_, {.flags = 0,
                                             .max_sets = 10,
                                             .pool_sizes = sizes,
                                             .debug_name = "Descriptor Pool"})
          .value();

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

    const VkDescriptorSetAllocateInfo global_set_alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &global_descriptor_set_layout_,
    };
    VK_CHECK(vkAllocateDescriptorSets(context_, &global_set_alloc_info,
                                      &frame.global_descriptor_set));

    const VkDescriptorSetAllocateInfo object_set_alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &object_descriptor_set_layout_,
    };
    VK_CHECK(vkAllocateDescriptorSets(context_, &object_set_alloc_info,
                                      &frame.object_descriptor_set));

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
      .size = sizeof(MeshPushConstants)};

  const VkDescriptorSetLayout set_layouts[] = {global_descriptor_set_layout_,
                                               object_descriptor_set_layout_};

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
          {.pipeline_layout = mesh_pipeline_layout_,
           .render_pass = render_pass_,
           .window_extend = to_extent2d(window_->resolution()),
           .debug_name = "Mesh Graphics Pipeline",
           .vertex_input_state_create_info =
               {.binding_descriptions = binding_descriptions,
                .attribute_descriptions = Vertex::attributes_descriptions()},
           .shader_stages = triangle_shader_stages})
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

void Renderer::render()
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

  draw_objects(cmd, render_objects_);

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
      materials_.emplace(std::move(name), Material{pipeline, layout});
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
  Assimp::Importer importer;

  const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate);
  if (!scene || !scene->HasMeshes()) {
    beyond::panic(fmt::format("Unable to load {}", filename));
  }
  const aiMesh* mesh = scene->mMeshes[0];

  std::vector<Vertex> vertices;
  for (unsigned i = 0; i != mesh->mNumVertices; i++) {
    const auto v = mesh->mVertices[i];
    const auto n = mesh->mNormals[i];
    // const aiVector3D t = mesh->mTextureCoords[0][i];
    vertices.push_back(Vertex{.position = {v.x, v.z, v.y},
                              .normal = {n.x, n.y, n.z},
                              .color = {n.x, n.y, n.z}});
  }

  std::vector<std::uint32_t> indices;
  for (unsigned i = 0; i != mesh->mNumFaces; i++)
    for (unsigned j = 0; j != 3; j++)
      indices.push_back(mesh->mFaces[i].mIndices[j]);

  return upload_mesh_data(context, vertices, indices);
}

[[nodiscard]] auto
Renderer::upload_mesh_data(vkh::Context& context,
                           std::span<const Vertex> vertices,
                           std::span<const std::uint32_t> indices) -> Mesh
{

  const auto index_buffer_size = indices.size() * sizeof(uint32_t);

  vkh::Buffer vertex_buffer =
      upload_buffer(vertices.size_bytes(), vertices.data(),
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
          .value();

  auto index_staging_buffer =
      vkh::create_buffer_from_data(context,
                                   {.size = index_buffer_size,
                                    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                    .debug_name = "Mesh Index Staging Buffer"},
                                   indices.data())
          .value();

  auto index_buffer =
      vkh::create_buffer(context, {.size = index_buffer_size,
                                   .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                   .debug_name = "Mesh Index Buffer"})
          .value();

  immediate_submit([=](VkCommandBuffer cmd) {
    const VkBufferCopy copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = index_buffer_size,
    };
    vkCmdCopyBuffer(cmd, index_staging_buffer.buffer, index_buffer.buffer, 1,
                    &copy);
  });

  vkh::destroy_buffer(context, index_staging_buffer);

  return Mesh{.vertex_buffer = vertex_buffer,
              .index_buffer = index_buffer,
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
                            std::span<RenderObject> objects)
{
  // Copy to object buffer
  void* object_data = nullptr;
  vmaMapMemory(context_.allocator(), current_frame().object_buffer.allocation,
               &object_data);

  auto* object_data_typed = static_cast<GPUObjectData*>(object_data);

  std::size_t object_count = objects.size();
  BEYOND_ENSURE(object_count <= max_object_count);
  for (std::size_t i = 0; i < objects.size(); ++i) {
    RenderObject& object = objects[i];
    object_data_typed[i].model = object.model_matrix;
  }

  vmaUnmapMemory(context_.allocator(),
                 current_frame().object_buffer.allocation);

  // Camera
  beyond::Mat4 view = beyond::look_at(beyond::Vec3{10.f, 5.f, 5.f},
                                      beyond::Vec3{0.0f, 0.0f, 0.0f},
                                      beyond::Vec3{0.0f, 1.0f, 0.0f});
  const auto res = window_->resolution();
  const float aspect =
      static_cast<float>(res.width) / static_cast<float>(res.height);

  using namespace beyond::literals;
  beyond::Mat4 projection = beyond::perspective(70._deg, aspect, 0.1f, 200.0f);

  GPUCameraData cam_data;
  cam_data.proj = projection;
  cam_data.view = view;
  cam_data.view_proj = projection * view;

  vkh::Buffer& camera_buffer = current_frame().camera_buffer;

  void* data = nullptr;
  vmaMapMemory(context_.allocator(), camera_buffer.allocation, &data);
  memcpy(data, &cam_data, sizeof(GPUCameraData));
  vmaUnmapMemory(context_.allocator(), camera_buffer.allocation);

  // Render objects
  for (std::size_t i = 0; i < object_count; ++i) {
    RenderObject& object = objects[i];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      object.material->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            object.material->pipeline_layout, 0, 1,
                            &current_frame().global_descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            object.material->pipeline_layout, 1, 1,
                            &current_frame().object_descriptor_set, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertex_buffer.buffer,
                           &offset);
    vkCmdBindIndexBuffer(cmd, object.mesh->index_buffer.buffer, 0,
                         VK_INDEX_TYPE_UINT32);

    const MeshPushConstants constants = {.transformation = object.model_matrix};

    vkCmdPushConstants(cmd, object.material->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);
    vkCmdDrawIndexed(cmd, object.mesh->index_count, 1, 0, 0,
                     static_cast<std::uint32_t>(i));
  }
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  for (auto& [_, mesh] : meshes_) {
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

  vkDestroyDescriptorSetLayout(context_, object_descriptor_set_layout_,
                               nullptr);
  vkDestroyDescriptorSetLayout(context_, global_descriptor_set_layout_,
                               nullptr);
  vkDestroyDescriptorPool(context_, descriptor_pool_, nullptr);

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

  constexpr VkCommandBufferBeginInfo cmd_begin_info = {
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