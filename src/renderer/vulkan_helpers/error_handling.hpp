#pragma once

#include <type_traits>

#include <beyond/types/expected.hpp>
#include <fmt/format.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x)                                                                                \
  do {                                                                                             \
    const VkResult err = x;                                                                        \
    if (err) {                                                                                     \
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
