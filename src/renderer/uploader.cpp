#include "uploader.hpp"

#include "vulkan_helpers/commands.hpp"
#include "vulkan_helpers/sync.hpp"

namespace charlie {

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>
{
  return vkh::create_fence(context, vkh::FenceCreateInfo{.debug_name = "Upload Fence"})
      .and_then([&](VkFence fence) -> vkh::Expected<UploadContext> {
        UploadContext upload_context;
        upload_context.fence = fence;

        const VkCommandPoolCreateInfo command_pool_create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = context.graphics_queue_family_index()};

        VKH_TRY(vkCreateCommandPool(context, &command_pool_create_info, nullptr,
                                    &upload_context.command_pool));

        return upload_context;
      });
}

void immediate_submit(vkh::Context& context, UploadContext& upload_context,
                      beyond::function_ref<void(VkCommandBuffer)> function)
{
  VkCommandBuffer cmd =
      vkh::allocate_command_buffer(context, {.command_pool = upload_context.command_pool,
                                             .debug_name = "Uploading Command Buffer"})
          .value();

  static constexpr VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  const VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};

  VK_CHECK(vkQueueSubmit(context.graphics_queue(), 1, &submit, upload_context.fence));
  VK_CHECK(vkWaitForFences(context, 1, &upload_context.fence, true, 9999999999));
  VK_CHECK(vkResetFences(context, 1, &upload_context.fence));
  VK_CHECK(vkResetCommandPool(context, upload_context.command_pool, 0));
}

} // namespace charlie