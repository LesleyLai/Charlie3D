#ifndef CHARLIE3D_UPLOADER_HPP
#define CHARLIE3D_UPLOADER_HPP

#include "vulkan_helpers/context.hpp"
#include <beyond/utils/function_ref.hpp>

namespace charlie {

struct UploadContext {
  VkFence fence = {};
  VkCommandPool command_pool = {};
};

auto init_upload_context(vkh::Context& context) -> vkh::Expected<UploadContext>;

void immediate_submit(vkh::Context& context, UploadContext& upload_context,
                      beyond::function_ref<void(VkCommandBuffer)> function);

} // namespace charlie

#endif // CHARLIE3D_UPLOADER_HPP
