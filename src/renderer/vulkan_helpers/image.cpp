#include "image.hpp"

#include "beyond/utils/bit_cast.hpp"
#include "beyond/utils/conversion.hpp"
#include "context.hpp"
#include "debug_utils.hpp"

namespace vkh {

auto create_image(vkh::Context& context, const ImageCreateInfo& image_create_info)
    -> Expected<Image>
{
  const VkImageCreateInfo vk_image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = image_create_info.flags,
      .imageType = image_create_info.image_type,
      .format = image_create_info.format.value,
      .extent = image_create_info.extent.value,
      .mipLevels = image_create_info.mip_levels,
      .arrayLayers = image_create_info.array_layers,
      .samples = image_create_info.samples,
      .tiling = image_create_info.tiling,
      .usage = image_create_info.usage.value,
      .sharingMode = image_create_info.sharing_mode,
      .queueFamilyIndexCount = beyond::to_u32(image_create_info.queue_fimily_indices.size()),
      .pQueueFamilyIndices = image_create_info.queue_fimily_indices.data(),
      .initialLayout = image_create_info.initial_layout,
  };

  static constexpr VmaAllocationCreateInfo image_alloc_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  Image image;
  VKH_TRY(vmaCreateImage(context.allocator(), &vk_image_create_info, &image_alloc_info,
                         &image.image, &image.allocation, nullptr));

  if (image_create_info.debug_name != nullptr &&
      set_debug_name(context, beyond::bit_cast<uint64_t>(image.image), VK_OBJECT_TYPE_IMAGE,
                     image_create_info.debug_name)) {
    report_fail_to_set_debug_name(image_create_info.debug_name);
  }

  return image;
}

void destroy_image(Context& context, Image& image)
{
  vmaDestroyImage(context.allocator(), image.image, image.allocation);
}

} // namespace vkh