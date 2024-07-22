#ifndef CHARLIE3D_PIPELINE_MANAGER_HPP
#define CHARLIE3D_PIPELINE_MANAGER_HPP

// The Pipeline manager implements shader hot-reloading. It monitors shader change and recreate
// pipelines when necessary.

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "../utils/file_watcher.hpp"
#include "../utils/prelude.hpp"

#include "../shader_compiler/shader_compiler.hpp"

#include <beyond/container/static_vector.hpp>
#include <beyond/utils/assert.hpp>
#include <beyond/utils/handle.hpp>

#include "../utils/string_map.hpp"
#include "../vulkan_helpers/blending.hpp"
#include "../vulkan_helpers/graphics_pipeline.hpp"
#include "../vulkan_helpers/initializers.hpp"

#include <vulkan/vulkan.h>

namespace charlie {

// A "virtual" shader handle
struct ShaderHandle : beyond::Handle<ShaderHandle, uintptr_t> {
  using Handle::Handle;
  friend class PipelineManager;
};

struct ShaderEntry {
  ShaderStage stage = {};
  VkShaderModule shader_module = VK_NULL_HANDLE;
  std::string file_path;
};

struct ComputePipelineHandle : beyond::Handle<ComputePipelineHandle> {
  using Handle::Handle;
  friend class PipelineManager;
};

struct GraphicsPipelineHandle : beyond::Handle<GraphicsPipelineHandle> {
  using Handle::Handle;
  friend class PipelineManager;
};

// Used internally to differentiate between diffrent types of pipelines
using PipelineHandle = std::variant<GraphicsPipelineHandle, ComputePipelineHandle>;

struct DepthBiasInfo {
  float constant_factor = 1.00f; // a scalar factor controlling the constant depth value added to
                                 // each fragment
  float clamp = 0.0f;            // maximum (or minimum) depth bias of a fragment
  float slope_factor =
      1.00f; // a scalar factor applied to a fragment’s slope in depth bias calculations
};

struct ShaderStageCreateInfo {
  ShaderHandle handle;
  const VkSpecializationInfo* p_specialization_info = nullptr;
};

struct RasterizationStateCreateInfo {
  VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
  VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
  // Enable depth bias if it is not nullopt
  beyond::optional<DepthBiasInfo> depth_bias_info = beyond::nullopt;
};

struct ComputePipelineCreateInfo {
  VkPipelineLayout layout = VK_NULL_HANDLE;
  ShaderStageCreateInfo stage;

  std::string debug_name;
};

struct GraphicsPipelineCreateInfo {
  VkPipelineLayout layout = VK_NULL_HANDLE;
  vkh::PipelineRenderingCreateInfo pipeline_rendering_create_info;
  vkh::PipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
  beyond::StaticVector<ShaderStageCreateInfo, 6> stages;
  RasterizationStateCreateInfo rasterization_state;
  vkh::PipelineDepthStencilStateCreateInfo depth_stencil_state;
  VkPipelineColorBlendAttachmentState color_blending = vkh::color_blend_attachment_disable();

  std::string debug_name;
};

class PipelineManager {
  VkDevice device_ = VK_NULL_HANDLE;
  ShaderCompiler shader_compiler_;
  std::unique_ptr<struct Shaders> shaders_;
  FileWatcher shader_file_watcher_;

  std::vector<VkPipeline> graphics_pipelines_;
  std::vector<VkPipeline> compute_pipelines_;
  std::unique_ptr<struct PipelineCreateInfoCache>
      pipeline_create_info_cache_; // Cached for pipeline recreation

  // An adjacency list from shader entry to pipelines that depends on those shader entries
  std::unordered_map<ShaderHandle, std::vector<PipelineHandle>> pipeline_dependency_map_;

  // An adjacency list from include files to shader entries that depends on those files
  StringHashMap<std::unordered_set<ShaderHandle>> header_dependency_map_;

public:
  explicit PipelineManager(VkDevice device);
  ~PipelineManager();

  PipelineManager(const PipelineManager&) = delete;
  auto operator=(const PipelineManager&) -> PipelineManager& = delete;

  // Check whether we need to rebuild some pipelines
  void update();

  [[nodiscard]] auto add_shader(beyond::ZStringView filename, ShaderStage stage) -> ShaderHandle;

  // Create a graphics pipeline
  [[nodiscard]] auto
  create_graphics_pipeline(const GraphicsPipelineCreateInfo& create_info) -> GraphicsPipelineHandle;

  [[nodiscard]] auto
  create_compute_pipeline(const ComputePipelineCreateInfo& create_info) -> ComputePipelineHandle;

  // Call vkCmdBindPipeline on a graphics or compute pipeline handle
  void cmd_bind_pipeline(VkCommandBuffer cmd, GraphicsPipelineHandle handle) const;
  void cmd_bind_pipeline(VkCommandBuffer cmd, ComputePipelineHandle handle) const;

private:
  void reload_shader(Ref<ShaderEntry> entry);
};

} // namespace charlie

#endif // CHARLIE3D_PIPELINE_MANAGER_HPP
