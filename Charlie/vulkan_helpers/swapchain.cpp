#include "swapchain.hpp"
#include "context.hpp"

#include "debug_utils.hpp"

#include "VkBootstrap.h"

#include <tracy/Tracy.hpp>

#include <span>

namespace {

void destroy(VkDevice device, VkSwapchainKHR swapchain,
             std::span<VkImageView> swapchain_image_views)
{
  if (device) {
    for (VkImageView image_view : swapchain_image_views) {
      vkDestroyImageView(device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }
}

} // namespace

namespace vkh {

Swapchain::Swapchain(Context& context, const SwapchainCreateInfo& create_info)
    : device_{context.device()}
{
  ZoneScoped;

  vkb::SwapchainBuilder swapchain_builder{context.physical_device(), context.device(),
                                          context.surface()};

  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(create_info.extent.width, create_info.extent.height)
          .set_old_swapchain(create_info.old_swapchain)
          .build()
          .value();

  swapchain_ = vkb_swapchain.swapchain;
  images_ = vkb_swapchain.get_images().value();
  image_views_ = vkb_swapchain.get_image_views().value();
  image_format_ = vkb_swapchain.image_format;

  for (size_t i = 0; i < images_.size(); ++i) {
    VK_CHECK(set_debug_name(context, images_[i], fmt::format("Swapchain Image {}", i)));
    VK_CHECK(set_debug_name(context, image_views_[i], fmt::format("Swapchain Image View {}", i)));
  }
}
auto Swapchain::acquire_next_image(VkSemaphore present_semaphore) -> VkResult
{
  ZoneScoped;

  return vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, present_semaphore, nullptr,
                               &current_image_index_);
}

Swapchain::~Swapchain()
{
  destroy(device_, swapchain_, image_views_);
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : device_{std::exchange(other.device_, {})}, swapchain_{std::exchange(other.swapchain_, {})},
      images_{std::exchange(other.images_, {})},
      image_views_{std::exchange(other.image_views_, {})},
      image_format_{std::exchange(other.image_format_, {})}
{
}

auto Swapchain::operator=(Swapchain&& other) & noexcept -> Swapchain&
{
  if (this != &other) {
    destroy(device_, swapchain_, image_views_);

    device_ = std::exchange(other.device_, {});
    swapchain_ = std::exchange(other.swapchain_, {});
    images_ = std::exchange(other.images_, {});
    image_views_ = std::exchange(other.image_views_, {});
    image_format_ = std::exchange(other.image_format_, {});
  }
  return *this;
}

} // namespace vkh
