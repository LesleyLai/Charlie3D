#pragma once

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vulkan.h>

#include "beyond/types/expected.hpp"
#include "beyond/utils/narrowing.hpp"

#include <cstdint>

#include "../window/window.hpp"
#include "error_handling.hpp"

namespace vkh {

struct AllocatedBuffer;

class Context {
  VkInstance instance_{};
  VkDebugUtilsMessengerEXT debug_messenger_{};
  VkSurfaceKHR surface_{};
  VkPhysicalDevice physical_device_{};
  VkPhysicalDeviceProperties gpu_properties_{};
  VkDevice device_{};
  VkQueue graphics_queue_{};
  VkQueue compute_queue_{};
  VkQueue transfer_queue_{};
  VkQueue present_queue_{};
  uint32_t graphics_queue_family_index_ = 0;
  uint32_t compute_queue_family_index_ = 0;
  uint32_t transfer_queue_family_index_ = 0;

  VmaAllocator allocator_{};

public:
  Context() = default;
  explicit Context(charlie::Window& window);
  ~Context();

  Context(const Context&) = delete;
  auto operator=(const Context&) -> Context& = delete;
  Context(Context&& other) noexcept;
  auto operator=(Context&& other) & noexcept -> Context&;

  BEYOND_FORCE_INLINE void wait_idle() noexcept { vkDeviceWaitIdle(device_); }

  [[nodiscard]] BEYOND_FORCE_INLINE auto instance() noexcept -> VkInstance { return instance_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto debug_messenger() noexcept -> VkDebugUtilsMessengerEXT
  {
    return debug_messenger_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto surface() noexcept -> VkSurfaceKHR { return surface_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto physical_device() noexcept -> VkPhysicalDevice
  {
    return physical_device_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto device() noexcept -> VkDevice { return device_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto graphics_queue() noexcept -> VkQueue
  {
    return graphics_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto present_queue() noexcept -> VkQueue
  {
    return present_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto compute_queue() noexcept -> VkQueue
  {
    return compute_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto transfer_queue() noexcept -> VkQueue
  {
    return transfer_queue_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto graphics_queue_family_index() const noexcept -> uint32_t
  {
    return graphics_queue_family_index_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto compute_queue_family_index() const noexcept -> uint32_t
  {
    return compute_queue_family_index_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto transfer_queue_family_index() const noexcept -> uint32_t
  {
    return transfer_queue_family_index_;
  }

  [[nodiscard]] BEYOND_FORCE_INLINE auto allocator() noexcept -> VmaAllocator { return allocator_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto gpu_properties() const noexcept
      -> const VkPhysicalDeviceProperties&
  {
    return gpu_properties_;
  }

  // Calculate required alignment based on minimum device offset alignment
  // Snippet from https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
  [[nodiscard]] auto align_uniform_buffer_size(size_t original_size) -> size_t
  {
    const size_t min_ubo_alignment = gpu_properties_.limits.minUniformBufferOffsetAlignment;
    size_t aligned_size = original_size;
    if (min_ubo_alignment > 0) {
      aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
    }
    return aligned_size;
  }

  BEYOND_FORCE_INLINE explicit operator bool() { return instance_ != nullptr; }

  BEYOND_FORCE_INLINE explicit(false) operator VkDevice() { return device_; }

  template <typename T = void> auto map(AllocatedBuffer& buffer) -> Expected<T*>
  {
    return map_impl(buffer).map([](void* ptr) { return beyond::narrow<T*>(ptr); });
  }

  void unmap(const AllocatedBuffer& buffer);

private:
  [[nodiscard]] auto map_impl(const AllocatedBuffer& buffer) -> Expected<void*>;
};

} // namespace vkh
