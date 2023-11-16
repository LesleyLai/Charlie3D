#include "descriptor_set_layout.hpp"
#include "debug_utils.hpp"

#include "../utils/prelude.hpp"
#include <volk.h>

namespace vkh {

auto create_descriptor_set_layout(VkDevice device, const DescriptorSetLayoutCreateInfo& create_info)
    -> Expected<VkDescriptorSetLayout>
{
  VkDescriptorSetLayoutCreateInfo vk_create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = create_info.flags,
      .bindingCount = narrow<u32>(create_info.bindings.size()),
      .pBindings = create_info.bindings.data(),
  };

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VKH_TRY(vkCreateDescriptorSetLayout(device, &vk_create_info, nullptr, &layout));

  if (set_debug_name(device, layout, create_info.debug_name)) {
    report_fail_to_set_debug_name(create_info.debug_name);
  }

  return layout;
}

} // namespace vkh