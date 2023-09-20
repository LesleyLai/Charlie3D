#include "shader_module.hpp"

#include "context.hpp"
#include "debug_utils.hpp"
#include "error_handling.hpp"

#include <beyond/utils/bit_cast.hpp>

#include <fmt/format.h>

namespace vkh {

[[nodiscard]] auto load_shader_module(Context& context, std::span<const uint32_t> buffer,
                                      const ShaderModuleCreateInfo& create_info)
    -> beyond::expected<VkShaderModule, VkResult>
{
  const VkShaderModuleCreateInfo vk_create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = buffer.size_bytes(),
      .pCode = buffer.data(),
  };

  VkShaderModule shader_module = VK_NULL_HANDLE;
  VKH_TRY(vkCreateShaderModule(context.device(), &vk_create_info, nullptr, &shader_module));

  if (set_debug_name(context, shader_module, create_info.debug_name)) {
    fmt::print("Cannot create debug name for {}\n", create_info.debug_name);
  }
  return shader_module;
}

} // namespace vkh
