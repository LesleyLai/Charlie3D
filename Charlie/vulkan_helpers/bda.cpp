#include "bda.hpp"

#include <volk.h>

namespace vkh {

[[nodiscard]] auto get_buffer_device_address(VkDevice device, VkBuffer buffer) -> VkDeviceAddress
{
  VkBufferDeviceAddressInfo device_address_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
  return vkGetBufferDeviceAddress(device, &device_address_info);
};

} // namespace vkh