#pragma once

#include <beyond/types/expected.hpp>

#include <vulkan/vulkan_core.h>

#include <fmt/format.h>

// TODO: fix this
#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) { fmt::print(stderr, "Vulkan error: {}\n", static_cast<int>(err)); }     \
  } while (0)

#define VKH_TRY(expr)                                                          \
  if (VkResult result = (expr); result != VK_SUCCESS) {                        \
    return beyond::make_unexpected(result);                                    \
  }

namespace vkh {

template <class T> using Expected = beyond::expected<T, VkResult>;

} // namespace vkh
