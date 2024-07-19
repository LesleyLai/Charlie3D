#ifndef CHARLIE3D_RENDER_PASS_HPP
#define CHARLIE3D_RENDER_PASS_HPP

#include <vulkan/vulkan_core.h>

namespace charlie {

struct FrameGraphRenderPass {
  virtual void pre_render() {}
  virtual void render(VkCommandBuffer cmd, VkImageView color_image_view) = 0;

  FrameGraphRenderPass() = default;
  virtual ~FrameGraphRenderPass() = default;

  FrameGraphRenderPass(const FrameGraphRenderPass&) = delete;
  auto operator=(const FrameGraphRenderPass&) -> FrameGraphRenderPass& = delete;
};

} // namespace charlie

#endif // CHARLIE3D_RENDER_PASS_HPP
