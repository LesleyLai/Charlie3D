#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include "beyond/utils/utils.hpp"

namespace vkh {

class Context;

class Swapchain;

struct SwapchainCreateInfo {
  VkExtent2D extent;
  VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
};

// Functions that contains `current_` in their name will have values updated after
// `acquire_next_image` returns
class Swapchain {
  VkDevice device_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImage> images_{};
  std::vector<VkImageView> image_views_{};
  VkFormat image_format_ = VK_FORMAT_UNDEFINED;

  std::uint32_t current_image_index_ = 0;

public:
  Swapchain() noexcept = default;
  Swapchain(Context& context, const SwapchainCreateInfo& create_info);
  ~Swapchain();
  Swapchain(const Swapchain&) = delete;
  auto operator=(const Swapchain&) & -> Swapchain& = delete;
  Swapchain(Swapchain&&) noexcept;
  auto operator=(Swapchain&&) & noexcept -> Swapchain&;

  auto acquire_next_image(VkSemaphore present_semaphore) -> VkResult;

  [[nodiscard]] BEYOND_FORCE_INLINE explicit(false) operator VkSwapchainKHR() const noexcept
  {
    return swapchain_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto get() noexcept -> VkSwapchainKHR { return swapchain_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto current_image_index() const noexcept -> std::uint32_t
  {
    return current_image_index_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto current_image() const noexcept -> VkImage
  {
    return images_[current_image_index_];
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto current_image_view() const noexcept -> VkImageView
  {
    return image_views_[current_image_index_];
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto image_format() const noexcept -> VkFormat
  {
    return image_format_;
  }
};

} // namespace vkh
