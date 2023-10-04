#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <beyond/utils/assert.hpp>

#include "error_handling.hpp"

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

  explicit(false) operator VkBuffer()
  {
    return buffer;
  }
};

auto create_buffer(vkh::Context& context, const BufferCreateInfo& buffer_create_info)
    -> Expected<AllocatedBuffer>;

auto create_buffer_from_data(vkh::Context& context, const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<AllocatedBuffer>;

template <typename T>
auto create_buffer_from_data(vkh::Context& context, const BufferCreateInfo& buffer_create_info,
                             const T* data) -> Expected<AllocatedBuffer>
{
  BEYOND_ENSURE(sizeof(T) <= buffer_create_info.size);
  return create_buffer_from_data(context, buffer_create_info, static_cast<const void*>(data));
}

void destroy_buffer(vkh::Context& context, AllocatedBuffer buffer);

} // namespace vkh
