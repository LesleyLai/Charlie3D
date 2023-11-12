#ifndef CHARLIE3D_PIPELINE_MANAGER_HPP
#define CHARLIE3D_PIPELINE_MANAGER_HPP

#include <filesystem>
#include <string>
#include <unordered_map>

#include "../utils/file_watcher.hpp"
#include "../utils/prelude.hpp"

#include "../shader_compiler/shader_compiler.hpp"

#include <beyond/container/static_vector.hpp>
#include <beyond/utils/assert.hpp>
#include <beyond/utils/handle.hpp>

#include "../vulkan_helpers/graphics_pipeline.hpp"
#include "../vulkan_helpers/shader_module.hpp"

#include <vulkan/vulkan.h>

namespace charlie {

// A "virtual" shader handle
struct ShaderHandle : beyond::Handle<ShaderHandle, uintptr_t> {
  using Handle::Handle;
  friend class PipelineManager;
};

} // namespace charlie

namespace charlie {

struct ShaderEntry {
  ShaderStage stage = {};
  VkShaderModule shader_module = VK_NULL_HANDLE;
};

// A "virtual" graphics pipeline handle is useful to deal with hot reloading
struct PipelineHandle : beyond::Handle<PipelineHandle> {
  using Handle::Handle;
  friend class PipelineManager;
};

struct GraphicsPipelineCreateInfo {
  VkPipelineLayout layout = VK_NULL_HANDLE;
  vkh::PipelineRenderingCreateInfo pipeline_rendering_create_info;
  std::string debug_name;
  vkh::PipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
  beyond::StaticVector<ShaderHandle, 6> shaders;
  VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
  VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
};

class PipelineManager {
  VkDevice device_ = VK_NULL_HANDLE;
  std::unique_ptr<struct Shaders> shaders_;
  FileWatcher shader_file_watcher_;

  std::vector<VkPipeline> pipelines_;
  // Cached for pipeline recreation
  std::vector<struct GraphicsPipelineCreateInfo> pipeline_create_infos_;

  // A map from shader entry to pipelines that depends on those shader entries
  std::unordered_map<ShaderHandle, std::vector<PipelineHandle>> pipeline_dependency_map_;

public:
  explicit PipelineManager(VkDevice device);
  ~PipelineManager();

  PipelineManager(const PipelineManager&) = delete;
  auto operator=(const PipelineManager&) -> PipelineManager& = delete;

  // Check whether we need to rebuild some pipelines
  void update();

  [[nodiscard]] auto add_shader(beyond::ZStringView filename, ShaderStage stage) -> ShaderHandle;

  // Create a graphics pipeline
  [[nodiscard]] auto create_graphics_pipeline(const GraphicsPipelineCreateInfo& create_info)
      -> PipelineHandle;

  [[nodiscard]] auto get_pipeline(PipelineHandle handle) const -> VkPipeline
  {
    return pipelines_.at(handle.value());
  }
};

} // namespace charlie

#endif // CHARLIE3D_PIPELINE_MANAGER_HPP
