#include "pipeline_manager.hpp"

#include "../vulkan_helpers/compute_pipeline.hpp"
#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/debug_utils.hpp"

#include <beyond/utils/assert.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>

#include <Tracy/Tracy.hpp>

#include "../utils/asset_path.hpp"
#include "../utils/string_map.hpp"
#include "../vulkan_helpers/blending.hpp"

namespace {

[[nodiscard]] constexpr auto to_VkShaderStageFlagBits(charlie::ShaderStage stage)
{
  using enum charlie::ShaderStage;
  switch (stage) {
  case vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case geometry:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case tess_control:
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  case tess_evaluation:
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  case task:
    return VK_SHADER_STAGE_TASK_BIT_EXT;
  case mesh:
    return VK_SHADER_STAGE_MESH_BIT_EXT;
  case raygen:
    return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  case any_hit:
    return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  case closest_hit:
    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  case miss:
    return VK_SHADER_STAGE_MISS_BIT_KHR;
  case intersection:
    return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
  case callable:
    return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
  }
  BEYOND_UNREACHABLE();
}

// Gets a shader entry from shader handle
[[nodiscard]] auto get_shader_entry(charlie::ShaderHandle handle) -> charlie::ShaderEntry&
{
  return *std::bit_cast<charlie::ShaderEntry*>(handle);
}

[[nodiscard]] auto to_VkPipelineShaderStageCreateInfo(charlie::ShaderStageCreateInfo shader_info)
    -> VkPipelineShaderStageCreateInfo
{
  const auto entry = get_shader_entry(shader_info.handle);

  return VkPipelineShaderStageCreateInfo{.sType =
                                             VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                         .stage = to_VkShaderStageFlagBits(entry.stage),
                                         .module = entry.shader_module,
                                         .pName = "main",
                                         .pSpecializationInfo = shader_info.p_specialization_info};
}

auto create_graphics_pipeline_impl(
    VkDevice device, const charlie::GraphicsPipelineCreateInfo& create_info) -> VkPipeline
{
  BEYOND_ENSURE(create_info.layout != VK_NULL_HANDLE);

  const auto vertex_binding_descriptions =
      create_info.vertex_input_state_create_info.binding_descriptions;
  const auto vertex_attribute_descriptions =
      create_info.vertex_input_state_create_info.attribute_descriptions;

  const VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = beyond::narrow<uint32_t>(vertex_binding_descriptions.size()),
      .pVertexBindingDescriptions = vertex_binding_descriptions.data(),
      .vertexAttributeDescriptionCount =
          beyond::narrow<uint32_t>(vertex_attribute_descriptions.size()),
      .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()};

  static constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = create_info.rasterization_state.polygon_mode,
      .cullMode = create_info.rasterization_state.cull_mode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
  };
  if (create_info.rasterization_state.depth_bias_info.has_value()) {
    const charlie::DepthBiasInfo depth_bias_info =
        create_info.rasterization_state.depth_bias_info.value();
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = depth_bias_info.constant_factor;
    rasterizer.depthBiasClamp = depth_bias_info.clamp;
    rasterizer.depthBiasSlopeFactor = depth_bias_info.slope_factor;
  }

  static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  const VkPipelineColorBlendAttachmentState& color_blend_attachment = create_info.color_blending;

  const VkPipelineColorBlendStateCreateInfo color_blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  static constexpr VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  const auto pipeline_rendering_create_info = create_info.pipeline_rendering_create_info;
  const VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .viewMask = pipeline_rendering_create_info.view_mask,
      .colorAttachmentCount =
          beyond::narrow<uint32_t>(pipeline_rendering_create_info.color_attachment_formats.size()),
      .pColorAttachmentFormats = pipeline_rendering_create_info.color_attachment_formats.data(),
      .depthAttachmentFormat = pipeline_rendering_create_info.depth_attachment_format,
      .stencilAttachmentFormat = pipeline_rendering_create_info.stencil_attachment_format,
  };

  static constexpr VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                      VK_DYNAMIC_STATE_SCISSOR};

  static constexpr VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = beyond::size(dynamic_states),
      .pDynamicStates = dynamic_states};

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_info(create_info.stages.size());
  std::ranges::transform(create_info.stages, shader_stage_create_info.begin(),
                         to_VkPipelineShaderStageCreateInfo);

  const VkGraphicsPipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &vk_pipeline_rendering_create_info,
      .stageCount = beyond::narrow<uint32_t>(shader_stage_create_info.size()),
      .pStages = shader_stage_create_info.data(),
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depth_stencil_state,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state_create_info,
      .layout = create_info.layout,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };

  VkPipeline pipeline{};
  VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr,
                                     &pipeline));
  VK_CHECK(vkh::set_debug_name(device, pipeline, create_info.debug_name));

  return pipeline;
}

auto create_compute_pipeline_impl(
    VkDevice device, const charlie::ComputePipelineCreateInfo& create_info) -> VkPipeline
{
  auto stage_info = to_VkPipelineShaderStageCreateInfo(create_info.stage);

  return vkh::create_compute_pipeline(
             device, nullptr,
             vkh::ComputePipelineCreateInfo{.stage = stage_info, .layout = create_info.layout})
      .value();
}

} // anonymous namespace

namespace charlie {

struct Shaders {
  // Map from file path to shader module
  StringHashMap<ShaderEntry> shaders;

  [[nodiscard]] auto find_shader_entry(std::string_view path) -> beyond::optional<ShaderEntry&>
  {
    if (auto itr = shaders.find(path); itr != shaders.end()) {
      return itr->second;
    } else {
      return beyond::nullopt;
    }
  }

  [[nodiscard]] auto add_shader(std::string path, ShaderEntry entry) -> ShaderHandle
  {
    auto [itr, inserted] = shaders.try_emplace(BEYOND_MOV(path), entry);
    BEYOND_ENSURE(inserted);

    return bit_cast<ShaderHandle>(&itr->second);
  }
};

struct PipelineCreateInfoCache {
  std::vector<GraphicsPipelineCreateInfo> graphics_pipeline_create_infos;
  std::vector<ComputePipelineCreateInfo> compute_pipeline_create_infos;

  [[nodiscard]] auto add(GraphicsPipelineCreateInfo create_info) -> GraphicsPipelineHandle
  {
    beyond::StaticVector<ShaderStageCreateInfo, 6> new_shader_info;
    for (auto shader_info : create_info.stages) {
      if (shader_info.p_specialization_info != nullptr) {
        auto* new_specialization_info = new VkSpecializationInfo{
            .mapEntryCount = shader_info.p_specialization_info->mapEntryCount,
            .dataSize = shader_info.p_specialization_info->dataSize,
        };
        auto* map_entries = new VkSpecializationMapEntry[new_specialization_info->mapEntryCount];
        std::copy_n(shader_info.p_specialization_info->pMapEntries,
                    shader_info.p_specialization_info->mapEntryCount, map_entries);
        new_specialization_info->pMapEntries = map_entries;

        auto* data = new u8[shader_info.p_specialization_info->dataSize];
        memcpy(data, shader_info.p_specialization_info->pData,
               shader_info.p_specialization_info->dataSize);
        new_specialization_info->pData = data;

        shader_info.p_specialization_info = new_specialization_info;
      }

      new_shader_info.push_back(shader_info);
    }

    create_info.stages = BEYOND_MOV(new_shader_info);
    graphics_pipeline_create_infos.push_back(create_info);
    return GraphicsPipelineHandle{narrow<u32>(graphics_pipeline_create_infos.size() - 1)};
  }

  [[nodiscard]] auto add(ComputePipelineCreateInfo create_info) -> ComputePipelineHandle
  {
    // TODO: specialization info

    compute_pipeline_create_infos.push_back(create_info);
    return ComputePipelineHandle{narrow<u32>(compute_pipeline_create_infos.size() - 1)};
  }

  [[nodiscard]] auto get(GraphicsPipelineHandle handle) const -> const GraphicsPipelineCreateInfo&
  {
    return graphics_pipeline_create_infos.at(handle.value());
  }

  [[nodiscard]] auto get(ComputePipelineHandle handle) const -> const ComputePipelineCreateInfo&
  {
    return compute_pipeline_create_infos.at(handle.value());
  }
};

PipelineManager::PipelineManager(VkDevice device)
    : device_{device}, shaders_{std::make_unique<Shaders>()},
      pipeline_create_info_cache_{std::make_unique<PipelineCreateInfoCache>()}
{
}

PipelineManager::~PipelineManager()
{
  for (auto pipeline : graphics_pipelines_) { vkDestroyPipeline(device_, pipeline, nullptr); }
  for (auto pipeline : compute_pipelines_) { vkDestroyPipeline(device_, pipeline, nullptr); }
}

void PipelineManager::reload_shader(Ref<ShaderEntry> entry)
{
  ShaderEntry& shader_entry = entry.get();

  SPDLOG_INFO("Reload {}", shader_entry.file_path);
  ShaderCompiler compiler;
  auto compilation_res =
      compiler.compile_shader_from_file(shader_entry.file_path, {.stage = shader_entry.stage});
  if (not compilation_res.has_value()) return; // has compilation error

  VkShaderModule new_shader_module = vkh::load_shader_module(device_, compilation_res.value().spirv,
                                                             {.debug_name = shader_entry.file_path})
                                         .value();

  // Replace shader
  shader_entry.shader_module = new_shader_module;

  // Update related pipelines!
  const auto shader_handle = std::bit_cast<ShaderHandle>(&shader_entry);
  {

    auto& pipelines = this->pipeline_dependency_map_.at(shader_handle);
    for (const auto pipeline_handle : pipelines) {
      // TODO: recreate pipeline asynchronously
      std::visit(
          [this](auto handle) {
            if constexpr (std::is_same_v<GraphicsPipelineHandle,
                                         std::remove_cv_t<decltype(handle)>>) {
              const GraphicsPipelineCreateInfo& create_info =
                  this->pipeline_create_info_cache_->get(handle);
              VkPipeline& pipeline = graphics_pipelines_.at(handle.value());
              vkDestroyPipeline(device_, pipeline, nullptr);
              pipeline = create_graphics_pipeline_impl(device_, create_info);

              SPDLOG_INFO("Recreate {}!", create_info.debug_name);
            } else if constexpr (std::is_same_v<ComputePipelineHandle,
                                                std::remove_cv_t<decltype(handle)>>) {
              const ComputePipelineCreateInfo& create_info =
                  this->pipeline_create_info_cache_->get(handle);
              VkPipeline& pipeline = compute_pipelines_.at(handle.value());
              vkDestroyPipeline(device_, pipeline, nullptr);
              pipeline = create_compute_pipeline_impl(device_, create_info);

              SPDLOG_INFO("Recreate {}!", create_info.debug_name);
            } else {
              static_assert([]() { return false; }());
            }
          },
          pipeline_handle);
    }
  }
}

[[nodiscard]] auto PipelineManager::add_shader(beyond::ZStringView filename,
                                               ShaderStage stage) -> ShaderHandle
{
  ZoneScoped;
  ZoneText(filename.c_str(), filename.size());

  const auto asset_path = get_asset_path();
  std::filesystem::path shader_path = asset_path / "shaders" / filename.c_str();

  shader_file_watcher_.add_watch(
      {.path = shader_path,
       .callback = [this](const std::filesystem::path& path, const FileAction action) {
         if (action == FileAction::modified) {
           this->shaders_->find_shader_entry(path.string()).map([&](ShaderEntry& entry) {
             reload_shader(ref(entry));
           });
         }
       }});

  std::string shader_path_str = shader_path.string();
  auto compilation_res =
      shader_compiler_.compile_shader_from_file(shader_path_str, {.stage = stage});
  BEYOND_ENSURE(compilation_res.has_value());

  VkShaderModule shader_module = vkh::load_shader_module(device_, compilation_res.value().spirv,
                                                         {.debug_name = shader_path_str})
                                     .value();

  ShaderHandle shader_handle =
      shaders_->add_shader(BEYOND_MOV(shader_path_str), ShaderEntry{
                                                            .stage = stage,
                                                            .shader_module = shader_module,
                                                            .file_path = shader_path_str,
                                                        });

  for (const std::string& include_file : compilation_res.value().include_files) {
    if (auto itr = header_dependency_map_.find(include_file); itr != header_dependency_map_.end()) {
      itr->second.insert(shader_handle);
    } else {
      header_dependency_map_.try_emplace(include_file, std::unordered_set{shader_handle});
    }

    // Add file watcher for include file
    shader_file_watcher_.add_watch(
        {.path = include_file,
         .callback = [this](const std::filesystem::path& path, const FileAction action) {
           if (action == FileAction::modified) {
             for (const ShaderHandle& shader_handle : header_dependency_map_.at(path.string())) {
               auto& entry = get_shader_entry(shader_handle);
               // TODO: need to find correct path
               reload_shader(ref(entry));
             }
           }
         }});
  }

  // Create entry for the shader in the pipeline handle
  pipeline_dependency_map_.try_emplace(shader_handle, std::vector<PipelineHandle>{});

  return shader_handle;
}

void PipelineManager::update()
{
  shader_file_watcher_.poll_notifications();
}

auto PipelineManager::create_graphics_pipeline(const GraphicsPipelineCreateInfo& create_info)
    -> GraphicsPipelineHandle
{
  ZoneScoped;
  if (not create_info.debug_name.empty()) {
    ZoneText(create_info.debug_name.c_str(), create_info.debug_name.size());
  }

  VkPipeline pipeline = create_graphics_pipeline_impl(device_, create_info);
  graphics_pipelines_.push_back(pipeline);
  const auto pipeline_handle = pipeline_create_info_cache_->add(create_info);
  BEYOND_ENSURE(narrow<u32>(graphics_pipelines_.size() - 1) == pipeline_handle.value());

  for (const ShaderStageCreateInfo& shader_info : create_info.stages) {
    pipeline_dependency_map_.at(shader_info.handle).push_back(pipeline_handle);
  }

  SPDLOG_INFO("{} created", create_info.debug_name);

  return pipeline_handle;
}

auto PipelineManager::create_compute_pipeline(const charlie::ComputePipelineCreateInfo& create_info)
    -> ComputePipelineHandle
{
  ZoneScoped;
  if (not create_info.debug_name.empty()) {
    ZoneText(create_info.debug_name.c_str(), create_info.debug_name.size());
  }

  VkPipeline pipeline = create_compute_pipeline_impl(device_, create_info);
  compute_pipelines_.push_back(pipeline);
  const auto pipeline_handle = pipeline_create_info_cache_->add(create_info);
  BEYOND_ENSURE(narrow<u32>(compute_pipelines_.size() - 1) == pipeline_handle.value());

  pipeline_dependency_map_.at(create_info.stage.handle).push_back(pipeline_handle);

  SPDLOG_INFO("{} created", create_info.debug_name);

  return pipeline_handle;
}

void PipelineManager::cmd_bind_pipeline(VkCommandBuffer cmd, GraphicsPipelineHandle handle) const
{
  VkPipeline pipeline = graphics_pipelines_.at(handle.value());
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void PipelineManager::cmd_bind_pipeline(VkCommandBuffer cmd, ComputePipelineHandle handle) const
{
  VkPipeline pipeline = compute_pipelines_.at(handle.value());
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

} // namespace charlie