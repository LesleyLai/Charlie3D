#ifndef CHARLIE3D_SHADOW_MAP_RENDERER_HPP
#define CHARLIE3D_SHADOW_MAP_RENDERER_HPP

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/image.hpp"
#include "pipeline_manager.hpp"
#include "render_pass.hpp"

#include <beyond/utils/copy_move.hpp>

namespace vkh {
class Context;
}

namespace charlie {

class Renderer;
class SamplerCache;

// Implements host-side code for shadow mapping
class ShadowMapRenderer {
  Renderer& renderer_;

  static constexpr uint32_t shadow_map_width_ = 4096;
  static constexpr uint32_t shadow_map_height_ = 4096;

  static constexpr VkFormat shadow_map_format_ = VK_FORMAT_D32_SFLOAT;
  vkh::AllocatedImage shadow_map_image_;
  VkImageView shadow_map_image_view_;
  VkSampler shadow_map_sampler_ = VK_NULL_HANDLE;

  VkPipelineLayout shadow_map_pipeline_layout_ = VK_NULL_HANDLE;
  GraphicsPipelineHandle shadow_map_pipeline_;

public:
  explicit ShadowMapRenderer(Renderer& renderer, Ref<SamplerCache> sampler_cache);
  ~ShadowMapRenderer();

  // Initialization of pipelines is deferred
  void init_pipeline();

  BEYOND_DELETE_COPY(ShadowMapRenderer)
  BEYOND_DELETE_MOVE(ShadowMapRenderer)

  [[nodiscard]] auto shadow_map_image_info() const -> VkDescriptorImageInfo
  {
    return {
        .sampler = shadow_map_sampler_,
        .imageView = shadow_map_image_view_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  void record_commands(VkCommandBuffer cmd);
};

} // namespace charlie

#endif // CHARLIE3D_SHADOW_MAP_RENDERER_HPP
