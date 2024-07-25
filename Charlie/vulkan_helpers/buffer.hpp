#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "beyond/utils/assert.hpp"

#include "error_handling.hpp"

#include <span>

namespace vkh {

class Context;

struct BufferCreateInfo {
  size_t size = 0;
  VkBufferUsageFlags usage = 0;
  VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_UNKNOWN;
  beyond::ZStringView debug_name;
};

struct [[nodiscard]] AllocatedBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info = {};

  explicit(false) operator VkBuffer() const
  {
    return buffer;
  } // NOLINT(google-explicit-constructor)
};

auto create_buffer(vkh::Context& context,
                   const BufferCreateInfo& buffer_create_info) -> Expected<AllocatedBuffer>;

// Note that this function does not create a staging buffer (and thus
// buffer_create_info.memory_usage can't be VMA_MEMORY_USAGE_GPU_ONLY).
auto create_buffer_from_data(vkh::Context& context, const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<AllocatedBuffer>;

// Create buffer based on a span on the CPU. If `buffer_create_info.size() == 0`, use the size of
// the span
template <typename T>
auto create_buffer_from_data(vkh::Context& context, BufferCreateInfo buffer_create_info,
                             std::span<const T> span) -> Expected<AllocatedBuffer>
{
  if (buffer_create_info.size == 0) { buffer_create_info.size = span.size_bytes(); }
  BEYOND_ENSURE(span.size_bytes() <= buffer_create_info.size);
  return create_buffer_from_data(context, buffer_create_info, span.data());
}

void destroy_buffer(vkh::Context& context, AllocatedBuffer buffer);

} // namespace vkh
