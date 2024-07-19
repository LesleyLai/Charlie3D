#pragma once

#include "error_handling.hpp"
#include "required_field.hpp"

namespace vkh {

struct ComputePipelineCreateInfo {
  const void* p_next = nullptr;
  VkPipelineCreateFlags flags = 0;
  RequiredField<VkPipelineShaderStageCreateInfo> stage;
  RequiredField<VkPipelineLayout> layout;
  VkPipeline base_pipeline_handle = VK_NULL_HANDLE;
  int32_t base_pipeline_index = 0;
  std::string debug_name;
};

[[nodiscard]] auto
create_compute_pipeline(VkDevice device, VkPipelineCache pipeline_cache,
                        ComputePipelineCreateInfo create_info) -> Expected<VkPipeline>;

} // namespace vkh