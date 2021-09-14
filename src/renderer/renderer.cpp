#include "renderer.hpp"

#include "vulkan_helpers/commands.hpp"
#include "vulkan_helpers/error_handling.hpp"
#include "vulkan_helpers/graphics_pipeline.hpp"
#include "vulkan_helpers/shader_module.hpp"
#include "vulkan_helpers/sync.hpp"

#include "mesh.hpp"

struct MeshPushConstants {
  beyond::Mat4 transformation;
};

namespace {

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

[[nodiscard]] auto init_command_pool(vkh::Context& context) -> VkCommandPool
{
  const VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context.graphics_queue_family_index()};

  VkCommandPool command_pool{};
  VK_CHECK(vkCreateCommandPool(context.device(), &command_pool_create_info,
                               nullptr, &command_pool));
  return command_pool;
}

[[nodiscard]] auto init_default_render_pass(vkh::Context& context,
                                            const vkh::Swapchain& swapchain)
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

  static constexpr VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
  };

  const VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
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
                                     VkRenderPass render_pass)
{
  Resolution res = window.resolution();
  VkFramebufferCreateInfo framebuffers_create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .renderPass = render_pass,
      .attachmentCount = 1,
      .width = res.width,
      .height = res.height,
      .layers = 1,
  };

  const auto swapchain_imagecount = swapchain.images().size();
  std::vector<VkFramebuffer> framebuffers(swapchain_imagecount);

  for (std::size_t i = 0; i < swapchain_imagecount; ++i) {
    framebuffers_create_info.pAttachments = &swapchain.image_views()[i];
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
      swapchain_{init_swapchain(context_, window)},
      command_pool_{init_command_pool(context_)},
      main_command_buffer_{
          vkh::allocate_command_buffer(context_,
                                       {
                                           .command_pool = command_pool_,
                                           .debug_name = "Main Command Buffer",
                                       })
              .value()},
      render_pass_{init_default_render_pass(context_, swapchain_)},
      framebuffers_{
          init_framebuffers(window, context_, swapchain_, render_pass_)}
{
  init_sync_structures();
  init_pipelines();
  load_mesh();
}

void Renderer::init_sync_structures()
{
  present_semaphore_ =
      vkh::create_semaphore(context_, {.debug_name = "Present Semaphore"})
          .value();
  render_semaphore_ =
      vkh::create_semaphore(context_, {.debug_name = "Render Semaphore"})
          .value();
  render_fence_ =
      vkh::create_fence(context_, {.debug_name = "Render Fence"}).value();
}

void Renderer::init_pipelines()
{
  static constexpr VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(MeshPushConstants)};

  static constexpr VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range,
  };
  VK_CHECK(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info,
                                  nullptr, &triangle_pipeline_layout_));

  auto triangle_vert_shader =
      vkh::load_shader_module_from_file(context_, "shaders/triangle.vert.spv",
                                        {.debug_name = "Vertex Shader"})
          .value();
  auto triangle_frag_shader =
      vkh::load_shader_module_from_file(context_, "shaders/triangle.frag.spv",
                                        {.debug_name = "Fragment Shader"})
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
  triangle_pipeline_ =
      vkh::create_graphics_pipeline(
          context_,
          {.pipeline_layout = triangle_pipeline_layout_,
           .render_pass = render_pass_,
           .window_extend = to_extent2d(window_->resolution()),
           .debug_name = "Triangle Graphics Pipeline",
           .vertex_input_state_create_info =
               {.binding_descriptions = binding_descriptions,
                .attribute_descriptions = Vertex::attributes_descriptions()},
           .shader_stages = triangle_shader_stages})
          .value();

  vkDestroyShaderModule(context_, triangle_vert_shader, nullptr);
  vkDestroyShaderModule(context_, triangle_frag_shader, nullptr);
}

void Renderer::load_mesh()
{
  Vertex triangle_vertices[3] = {
      {.position = {1.f, 1.f, 0.0f}, .color = {1.f, 0.f, 0.0f}},
      {.position = {-1.f, 1.f, 0.0f}, .color = {0.f, 1.f, 0.0f}},
      {.position = {0.f, -1.f, 0.0f}, .color = {0.f, 0.f, 1.0f}}};

  triangle_buffer_ =
      vkh::create_buffer_from_data(context_,
                                   {.size = sizeof(triangle_vertices),
                                    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                    .debug_name = "Triangle Mesh"},
                                   triangle_vertices)
          .value();
}

void Renderer::render()
{
  // wait until the GPU has finished rendering the last frame.
  VK_CHECK(vkWaitForFences(context_, 1, &render_fence_, true, 1e9));
  VK_CHECK(vkResetFences(context_, 1, &render_fence_));

  std::uint32_t swapchain_image_index = 0;
  VK_CHECK(vkAcquireNextImageKHR(context_, swapchain_, 1e9, present_semaphore_,
                                 nullptr, &swapchain_image_index));

  VK_CHECK(vkResetCommandBuffer(main_command_buffer_, 0));

  VkCommandBuffer cmd = main_command_buffer_;
  constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  VkClearValue clear_value = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};

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
      .clearValueCount = 1,
      .pClearValues = &clear_value,
  };

  vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle_pipeline_);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &triangle_buffer_.buffer, &offset);

  beyond::Mat4 view = beyond::look_at(beyond::Vec3{0.f, 0.f, -2.f},
                                      beyond::Vec3{0.0f, 0.0f, 0.0f},
                                      beyond::Vec3{0.0f, 1.0f, 0.0f});
  //  camera projection
  const auto res = window_->resolution();
  const float aspect =
      static_cast<float>(res.width) / static_cast<float>(res.height);

  beyond::Mat4 projection =
      beyond::perspective(beyond::Degree{70.f}, aspect, 0.1f, 200.0f);
  const beyond::Mat4 model = beyond::rotate_y(
      beyond::Degree{static_cast<float>(frame_number_) * 0.4f});

  const MeshPushConstants constants = {.transformation =
                                           projection * view * model};

  vkCmdPushConstants(cmd, triangle_pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(MeshPushConstants), &constants);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  static constexpr VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  const VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &present_semaphore_,
      .pWaitDstStageMask = &waitStage,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &render_semaphore_,
  };

  VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit, render_fence_));

  VkSwapchainKHR swapchain = swapchain_.get();
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &render_semaphore_,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &swapchain_image_index,
  };

  VK_CHECK(vkQueuePresentKHR(graphics_queue_, &present_info));

  ++frame_number_;
}

Renderer::~Renderer()
{
  vkDeviceWaitIdle(context_);

  vkh::destroy_buffer(context_, triangle_buffer_);

  vkDestroyPipeline(context_, triangle_pipeline_, nullptr);
  vkDestroyPipelineLayout(context_, triangle_pipeline_layout_, nullptr);

  vkDestroyFence(context_, render_fence_, nullptr);
  vkDestroySemaphore(context_, render_semaphore_, nullptr);
  vkDestroySemaphore(context_, present_semaphore_, nullptr);

  for (auto framebuffer : framebuffers_) {
    vkDestroyFramebuffer(context_, framebuffer, nullptr);
  }
  vkDestroyRenderPass(context_, render_pass_, nullptr);
  vkDestroyCommandPool(context_, command_pool_, nullptr);
}

} // namespace charlie