#include "uploader.hpp"

#include "../asset_handling/cpu_image.hpp"
#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/image.hpp"
#include "../vulkan_helpers/initializers.hpp"

#include <beyond/utils/defer.hpp>

#include <tracy/Tracy.hpp>

namespace charlie {

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

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>
{
  ZoneScoped;

  return vkh::create_fence(context, vkh::FenceCreateInfo{.debug_name = "Upload Fence"})
      .and_then([&](VkFence fence) -> vkh::Expected<UploadContext> {
        return vkh::create_command_pool(
                   context,
                   vkh::CommandPoolCreateInfo{
                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                       .queue_family_index = context.graphics_queue_family_index(),
                       .debug_name = "Upload Command Pool",
                   })
            .map([&](VkCommandPool command_pool) {
              return UploadContext{
                  .fence = fence,
                  .command_pool = command_pool,
              };
            });
      });
}

void immediate_submit(vkh::Context& context, const UploadContext& upload_context,
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

auto upload_buffer(vkh::Context& context, const UploadContext& upload_context,
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

auto upload_image(vkh::Context& context, const UploadContext& upload_context,
                  const charlie::CPUImage& cpu_image,
                  const ImageUploadInfo& upload_info) -> vkh::AllocatedImage
{

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
      .width = beyond::narrow<u32>(cpu_image.width),
      .height = beyond::narrow<u32>(cpu_image.height),
      .depth = 1,
  };

  const u32 mip_levels = upload_info.mip_levels;
  const bool need_generate_mipmap = mip_levels > 1;

  const auto image_debug_name =
      cpu_image.name.empty() ? fmt::format("{} Image", cpu_image.name) : "Image";
  vkh::AllocatedImage allocated_image = [&]() {
    auto image = vkh::create_image(context, vkh::ImageCreateInfo{
                                                .format = upload_info.format,
                                                .extent = image_extent,
                                                .usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                .mip_levels = mip_levels,
                                                .debug_name = image_debug_name,
                                            });

    if (not image.has_value()) {
      beyond::panic(fmt::format("Failed to create image: {}", vkh::to_string(image.error())));
    }

    return image.value();
  }();

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
      vkGetPhysicalDeviceFormatProperties(context.physical_device(), upload_info.format,
                                          &format_properties);
      BEYOND_ENSURE(format_properties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

      cmd_generate_mipmap(cmd, allocated_image.image, Resolution{cpu_image.width, cpu_image.height},
                          mip_levels);
    }
  });
  return allocated_image;
}

} // namespace charlie