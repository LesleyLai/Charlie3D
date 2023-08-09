#include "imgui_render_pass.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <vulkan/vulkan.h>

#include "renderer.hpp"

namespace charlie {

ImguiRenderPass::ImguiRenderPass(Renderer& renderer, VkFormat color_attachment_format)
    : renderer_{&renderer}
{
  auto& context = renderer_->context();

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

  VK_CHECK(vkCreateDescriptorPool(context.device(), &pool_info, nullptr, &descriptor_pool_));

  // 2: initialize imgui library

  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForVulkan(renderer_->window().glfw_window(), true);

  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = context.instance(),
      .PhysicalDevice = context.physical_device(),
      .Device = context.device(),
      .QueueFamily = context.graphics_queue_family_index(),
      .Queue = context.graphics_queue(),
      .PipelineCache = VK_NULL_HANDLE,
      .DescriptorPool = descriptor_pool_,
      .MinImageCount = 3,
      .ImageCount = 3,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,

      .UseDynamicRendering = true,
      .ColorAttachmentFormat = color_attachment_format,

      .CheckVkResultFn = [](VkResult result) { VK_CHECK(result); },
  };
  ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

  // execute a gpu command to upload imgui font textures
  renderer_->immediate_submit(
      [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
  ImGui_ImplVulkan_DestroyFontUploadObjects();
}

ImguiRenderPass::~ImguiRenderPass()
{
  vkDestroyDescriptorPool(renderer_->context(), descriptor_pool_, nullptr);
}

void ImguiRenderPass::pre_render()
{
  ImGui::Render();
}

void ImguiRenderPass::render(VkCommandBuffer cmd, VkImageView image_view)
{
  const VkRenderingAttachmentInfo gui_color_attachments_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = image_view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  const VkRect2D render_area{
      .offset =
          {
              .x = 0,
              .y = 0,
          },
      .extent = to_extent2d(renderer_->window().resolution()),
  };
  const VkRenderingInfo gui_render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = render_area,
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &gui_color_attachments_info,
  };
  vkCmdBeginRendering(cmd, &gui_render_info);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}

} // namespace charlie