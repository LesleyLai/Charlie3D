#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include <beyond/utils/utils.hpp>

namespace vkh {

class Context;

class Swapchain;

struct SwapchainCreateInfo {
  VkExtent2D extent;
  VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
};

class Swapchain {
  VkDevice device_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImage> images_{};
  std::vector<VkImageView> image_views_{};
  VkFormat image_format_ = VK_FORMAT_UNDEFINED;

public:
  Swapchain() noexcept = default;
  Swapchain(Context& context, const SwapchainCreateInfo& create_info);
  ~Swapchain();
  Swapchain(const Swapchain&) = delete;
  auto operator=(const Swapchain&) & -> Swapchain& = delete;
  Swapchain(Swapchain&&) noexcept;
  auto operator=(Swapchain&&) & noexcept -> Swapchain&;

  [[nodiscard]] BEYOND_FORCE_INLINE explicit(false) operator VkSwapchainKHR() const noexcept
  {
    return swapchain_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto get() noexcept -> VkSwapchainKHR
  {
    return swapchain_;
  }
  [[nodiscard]] BEYOND_FORCE_INLINE auto images() const noexcept -> std::span<const VkImage>
  {
    return images_;
  }
  [[nodiscard]] BEYOND_FORCE_INLINE auto image_views() const noexcept
      -> const std::span<const VkImageView>
  {
    return image_views_;
  }
  [[nodiscard]] BEYOND_FORCE_INLINE auto image_format() const noexcept -> VkFormat
  {
    return image_format_;
  }
};

} // namespace vkh
