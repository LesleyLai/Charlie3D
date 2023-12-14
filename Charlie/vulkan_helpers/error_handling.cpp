#include "error_handling.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

namespace vkh {

[[nodiscard]] auto to_string(VkResult result) -> const char*
{
  return string_VkResult(result);
}

} // namespace vkh