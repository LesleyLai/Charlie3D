#include "image_view.hpp"

#include "beyond/utils/bit_cast.hpp"
#include "context.hpp"
#include "debug_utils.hpp"

namespace vkh {

auto create_image_view(vkh::Context& context, const ImageViewCreateInfo& image_view_create_info)
    -> Expected<VkImageView>
{
  const VkImageViewCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                          .pNext = nullptr,
                                          .flags = image_view_create_info.flags,
                                          .image = image_view_create_info.image.value,
                                          .viewType = image_view_create_info.view_type,
                                          .format = image_view_create_info.format.value,
                                          .components = image_view_create_info.components,
                                          .subresourceRange =
                                              image_view_create_info.subresource_range.value};

  VkImageView image_view = VK_NULL_HANDLE;
  VKH_TRY(vkCreateImageView(context, &create_info, nullptr, &image_view));

  if (image_view_create_info.debug_name != nullptr &&
      set_debug_name(context, beyond::bit_cast<uint64_t>(image_view), VK_OBJECT_TYPE_IMAGE_VIEW,
                     image_view_create_info.debug_name)) {
    report_fail_to_set_debug_name(image_view_create_info.debug_name);
  }

  return image_view;
}

} // namespace vkh