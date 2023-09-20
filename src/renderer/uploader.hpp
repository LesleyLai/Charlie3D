#ifndef CHARLIE3D_UPLOADER_HPP
#define CHARLIE3D_UPLOADER_HPP

#include "vulkan_helpers/context.hpp"
#include <beyond/utils/function_ref.hpp>
#include <iterator>
#include <span>

namespace charlie {

struct UploadContext {
  VkFence fence = {};
  VkCommandPool command_pool = {};
};

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>;

void immediate_submit(vkh::Context& context, UploadContext& upload_context,
                      beyond::function_ref<void(VkCommandBuffer)> function);

auto upload_buffer(vkh::Context& context, UploadContext& upload_context,
                   std::span<const std::byte> data, VkBufferUsageFlags usage,
                   const char* debug_name = "") -> vkh::Expected<vkh::AllocatedBuffer>;

template <class Container>
auto upload_buffer(vkh::Context& context, UploadContext& upload_context, const Container& buffer,
                   VkBufferUsageFlags usage, const char* debug_name = "")
    -> vkh::Expected<vkh::AllocatedBuffer>
  requires(std::contiguous_iterator<typename Container::iterator>)
{
  return charlie::upload_buffer(context, upload_context, std::as_bytes(std::span{buffer}), usage,
                                debug_name);
}

} // namespace charlie

#endif // CHARLIE3D_UPLOADER_HPP
