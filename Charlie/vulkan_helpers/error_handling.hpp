#pragma once

#include <type_traits>

#include "beyond/types/expected.hpp"
#include "beyond/utils/zstring_view.hpp"
#include <fmt/format.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x)                                                                                \
  do {                                                                                             \
    const VkResult err = x;                                                                        \
    if (err < 0) { /* Only negative codes are runtime errors */                                    \
      fmt::print(stderr, "Vulkan error: {} [{}] at {}:{}\n",                                       \
                 static_cast<std::underlying_type_t<VkResult>>(err), vkh::to_string(err),          \
                 __FILE__, __LINE__);                                                              \
    }                                                                                              \
  } while (0)

#define VKH_TRY(expr)                                                                              \
  if (const VkResult result = (expr); result != VK_SUCCESS) {                                      \
    return beyond::make_unexpected(result);                                                        \
  }

namespace vkh {

template <class T> using Expected = beyond::expected<T, VkResult>;

[[nodiscard]] auto to_string(VkResult result) -> const char*;

} // namespace vkh
