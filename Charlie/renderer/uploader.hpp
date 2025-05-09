#ifndef CHARLIE3D_UPLOADER_HPP
#define CHARLIE3D_UPLOADER_HPP

#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/image.hpp"

#include <beyond/utils/function_ref.hpp>
#include <iterator>
#include <span>

namespace charlie {

/*
 * Context to upload resources to the GPU
 */

struct UploadContext {
  VkFence fence = {};
  VkCommandPool command_pool = {};
};

struct ImageUploadInfo {
  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
  u32 mip_levels = 1; // Generate mipmaps if mip_level > 1
};

struct CPUImage;

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>;

void immediate_submit(vkh::Context& context, const UploadContext& upload_context,
                      beyond::function_ref<void(VkCommandBuffer)> function);

auto upload_buffer(vkh::Context& context, const UploadContext& upload_context,
                   std::span<const std::byte> data, VkBufferUsageFlags usage,
                   beyond::ZStringView debug_name = "") -> vkh::Expected<vkh::AllocatedBuffer>;

template <class Container>
auto upload_buffer(vkh::Context& context, const UploadContext& upload_context,
                   const Container& buffer, VkBufferUsageFlags usage,
                   beyond::ZStringView debug_name = "") -> vkh::Expected<vkh::AllocatedBuffer>
  requires(std::contiguous_iterator<typename Container::iterator>)
{
  return charlie::upload_buffer(context, upload_context, std::as_bytes(std::span{buffer}), usage,
                                debug_name);
}

[[nodiscard]]
auto upload_image(vkh::Context& context, const UploadContext& upload_context,
                  const charlie::CPUImage& cpu_image,
                  const ImageUploadInfo& upload_info) -> vkh::AllocatedImage;

} // namespace charlie

#endif // CHARLIE3D_UPLOADER_HPP
