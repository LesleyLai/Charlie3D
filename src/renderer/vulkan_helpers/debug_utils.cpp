#include "debug_utils.hpp"

#include "context.hpp"

#include <fmt/format.h>

namespace vkh {

[[nodiscard]] auto set_debug_name(Context& context, uint64_t object_handle,
                                  VkObjectType object_type, beyond::ZStringView name) noexcept
    -> VkResult
{
  if (name.empty()) { return VK_SUCCESS; }

  const VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = object_type,
      .objectHandle = object_handle,
      .pObjectName = name.c_str()};
  return context.functions().setDebugUtilsObjectNameEXT(context.device(), &name_info);
}

void report_fail_to_set_debug_name(beyond::ZStringView name) noexcept
{
  fmt::print("Cannot create debug name for {}\n", name);
}

} // namespace vkh
