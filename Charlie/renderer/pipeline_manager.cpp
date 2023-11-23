#include "pipeline_manager.hpp"
#include "../utils/configuration.hpp"

#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/debug_utils.hpp"

#include <beyond/utils/assert.hpp>
#include <spdlog/spdlog.h>

#include <Tracy/Tracy.hpp>

#include "../utils/string_map.hpp"

namespace {

[[nodiscard]] constexpr auto to_VkShaderStageFlagBits(charlie::ShaderStage stage)
{
  using enum charlie::ShaderStage;
  switch (stage) {
  case vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  BEYOND_UNREACHABLE();
}

[[nodiscard]] constexpr auto to_VkPipelineShaderStageCreateInfo(charlie::ShaderEntry entry)
    -> VkPipelineShaderStageCreateInfo
{
  return VkPipelineShaderStageCreateInfo{.sType =
                                             VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                         .stage = to_VkShaderStageFlagBits(entry.stage),
                                         .module = entry.shader_module,
                                         .pName = "main"};
}

auto create_graphics_pipeline_impl(VkDevice device,
                                   const charlie::GraphicsPipelineCreateInfo& create_info)
    -> VkPipeline
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

  const VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = create_info.polygon_mode,
      .cullMode = create_info.cull_mode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  static constexpr VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  static constexpr VkPipelineColorBlendStateCreateInfo color_blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  static constexpr VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
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

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_info(create_info.shaders.size());
  std::ranges::transform(
      create_info.shaders, shader_stage_create_info.begin(), [](charlie::ShaderHandle handle) {
        return to_VkPipelineShaderStageCreateInfo(*std::bit_cast<charlie::ShaderEntry*>(handle));
      });

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

PipelineManager::PipelineManager(VkDevice device)
    : device_{device}, shaders_{std::make_unique<Shaders>()}
{
}

PipelineManager::~PipelineManager()
{
  for (auto pipeline : pipelines_) { vkDestroyPipeline(device_, pipeline, nullptr); }
}

[[nodiscard]] auto PipelineManager::add_shader(beyond::ZStringView filename, ShaderStage stage)
    -> ShaderHandle
{
  ZoneScoped;
  ZoneText(filename.c_str(), filename.size());

  const auto asset_path = Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
  std::filesystem::path shader_directory = asset_path / "shaders";
  std::filesystem::path shader_path = shader_directory / filename.c_str();

  shader_file_watcher_.add_watch(
      {.directory = shader_directory, .callback = [this](const std::filesystem::path& path) {
         this->shaders_->find_shader_entry(path.string()).map([&](ShaderEntry& entry) {
           SPDLOG_INFO("{} changed", path.string());
           ShaderCompiler compiler;
           std::string shader_path_str = path.string();
           auto compilation_res =
               compiler.compile_shader_from_file(shader_path_str, {.stage = entry.stage});
           if (not compilation_res.has_value()) return; // has compilation error

           VkShaderModule new_shader_module =
               vkh::load_shader_module(device_, compilation_res.value().spirv,
                                       {.debug_name = shader_path_str})
                   .value();

           // Replace shader
           entry.shader_module = new_shader_module;

           // Update related pipelines!
           const auto shader_handle = std::bit_cast<ShaderHandle>(&entry);
           {
             beyond::optional<std::vector<PipelineHandle>&> pipelines_opt =
                 this->pipeline_dependency_map_.at(shader_handle);
             BEYOND_ENSURE(pipelines_opt.has_value());

             std::vector<PipelineHandle>& pipelines = pipelines_opt.value();
             for (const auto pipeline_handle : pipelines) {
               // TODO: recreate pipeline asynchronously
               const auto& create_info = this->pipeline_create_infos_.at(pipeline_handle.value());

               VkPipeline& pipeline = pipelines_.at(pipeline_handle.value());
               vkDestroyPipeline(device_, pipeline, nullptr);
               pipeline = create_graphics_pipeline_impl(device_, create_info);

               SPDLOG_INFO("Recreate {}!", create_info.debug_name);
             }
           }
         });
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
                                                        });

  // Create entry for the shader in the pipeline handle
  pipeline_dependency_map_.try_emplace(shader_handle, std::vector<PipelineHandle>{});

  return shader_handle;
}

void PipelineManager::update()
{
  shader_file_watcher_.poll_notifications();
}

auto PipelineManager::create_graphics_pipeline(const GraphicsPipelineCreateInfo& create_info)
    -> PipelineHandle
{
  VkPipeline pipeline = create_graphics_pipeline_impl(device_, create_info);

  pipelines_.push_back(pipeline);
  pipeline_create_infos_.push_back(create_info);

  const auto pipeline_handle = PipelineHandle{narrow<u32>(pipelines_.size() - 1)};

  for (ShaderHandle shader : create_info.shaders) {
    pipeline_dependency_map_.at(shader).push_back(pipeline_handle);
  }

  return pipeline_handle;
}

} // namespace charlie