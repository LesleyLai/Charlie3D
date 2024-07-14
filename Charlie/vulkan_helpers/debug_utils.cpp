#include "debug_utils.hpp"

#include "context.hpp"

#include <fmt/format.h>

namespace vkh {

[[nodiscard]] auto set_debug_name(VkDevice device, uint64_t object_handle, VkObjectType object_type,
                                  beyond::ZStringView name) noexcept -> VkResult
{
  if (name.empty()) { return VK_SUCCESS; }

  const VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = object_type,
      .objectHandle = object_handle,
      .pObjectName = name.c_str()};
  return vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

void report_fail_to_set_debug_name(beyond::ZStringView name) noexcept
{
  fmt::print("Cannot create debug name for {}\n", name);
}

void cmd_begin_debug_utils_label(VkCommandBuffer cmd, beyond::ZStringView label_name,
                                 std::array<float, 4> color)
{
  VkDebugUtilsLabelEXT label_info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                                  .pNext = nullptr,
                                  .pLabelName = label_name.c_str()};
  std::ranges::copy(color, label_info.color);
  vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
}

void cmd_end_debug_utils_label(VkCommandBuffer cmd)
{
  vkCmdEndDebugUtilsLabelEXT(cmd);
}

} // namespace vkh
