#include "uploader.hpp"

#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/commands.hpp"
#include "../vulkan_helpers/sync.hpp"

#include <beyond/utils/defer.hpp>

#include <tracy/Tracy.hpp>

namespace charlie {

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>
{
  ZoneScoped;

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
  ZoneScoped;

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

auto upload_buffer(vkh::Context& context, UploadContext& upload_context,
                   std::span<const std::byte> data, VkBufferUsageFlags usage,
                   beyond::ZStringView debug_name) -> vkh::Expected<vkh::AllocatedBuffer>
{
  const auto size = beyond::narrow<uint32_t>(data.size());

  BEYOND_ENSURE(size > 0);

  return vkh::create_buffer(context, {.size = size,
                                      .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                      .debug_name = fmt::format("{} Buffer", debug_name)})
      .and_then([=, &context, &upload_context](vkh::AllocatedBuffer gpu_buffer) {
        auto vertex_staging_buffer =
            vkh::create_buffer_from_data(
                context,
                {.size = size,
                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                 .debug_name = fmt::format("{} Staging Buffer", debug_name)},
                data.data())
                .value();
        BEYOND_DEFER(vkh::destroy_buffer(context, vertex_staging_buffer));

        immediate_submit(context, upload_context, [=](VkCommandBuffer cmd) {
          const VkBufferCopy copy = {
              .srcOffset = 0,
              .dstOffset = 0,
              .size = size,
          };
          vkCmdCopyBuffer(cmd, vertex_staging_buffer.buffer, gpu_buffer.buffer, 1, &copy);
        });
        return vkh::Expected<vkh::AllocatedBuffer>(gpu_buffer);
      });
}

} // namespace charlie