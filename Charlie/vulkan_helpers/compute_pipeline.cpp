#include "compute_pipeline.hpp"
#include "debug_utils.hpp"

#include <volk.h>

namespace vkh {

auto create_compute_pipeline(VkDevice device, VkPipelineCache pipeline_cache,
                             ComputePipelineCreateInfo create_info) -> Expected<VkPipeline>
{
  const VkComputePipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = create_info.p_next,
      .flags = create_info.flags,
      .stage = create_info.stage,
      .layout = create_info.layout,
      .basePipelineHandle = create_info.base_pipeline_handle,
      .basePipelineIndex = create_info.base_pipeline_index,
  };

  VkPipeline pipeline = VK_NULL_HANDLE;
  VKH_TRY(vkCreateComputePipelines(device, pipeline_cache, 1, &pipeline_create_info, nullptr,
                                   &pipeline));
  VKH_TRY(vkh::set_debug_name(device, pipeline, create_info.debug_name));

  return pipeline;
}

} // namespace vkh