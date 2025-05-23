
#include "buffer.hpp"
#include "context.hpp"
#include "debug_utils.hpp"

namespace vkh {

auto create_buffer(vkh::Context& context,
                   const BufferCreateInfo& buffer_create_info) -> Expected<AllocatedBuffer>
{
  const VkBufferCreateInfo vk_buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = buffer_create_info.size,
      .usage = buffer_create_info.usage,
  };

  const VmaAllocationCreateInfo vma_alloc_info{.usage = buffer_create_info.memory_usage};

  AllocatedBuffer allocated_buffer;
  VKH_TRY(vmaCreateBuffer(context.allocator(), &vk_buffer_create_info, &vma_alloc_info,
                          &allocated_buffer.buffer, &allocated_buffer.allocation,
                          &allocated_buffer.allocation_info));

  if (not buffer_create_info.debug_name.empty() &&
      set_debug_name(context, allocated_buffer.buffer, buffer_create_info.debug_name)) {
    report_fail_to_set_debug_name(buffer_create_info.debug_name);
  }

  return allocated_buffer;
}

auto create_buffer_from_data(vkh::Context& context, const BufferCreateInfo& buffer_create_info,
                             const void* data) -> Expected<AllocatedBuffer>
{
  return create_buffer(context, buffer_create_info)
      .and_then([&](AllocatedBuffer buffer) -> Expected<AllocatedBuffer> {
        BEYOND_EXPECTED_ASSIGN(void*, buffer_ptr, context.map(buffer));
        std::memcpy(buffer_ptr, data, buffer_create_info.size);
        context.unmap(buffer);
        return buffer;
      });
}

void destroy_buffer(vkh::Context& context, AllocatedBuffer buffer)
{
  vmaDestroyBuffer(context.allocator(), buffer.buffer, buffer.allocation);
}

} // namespace vkh