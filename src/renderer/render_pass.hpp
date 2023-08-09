#ifndef CHARLIE3D_RENDER_PASS_HPP
#define CHARLIE3D_RENDER_PASS_HPP

namespace charlie {

struct FrameGraphRenderPass {
  virtual void pre_render() {}
  virtual void render(VkCommandBuffer cmd, VkImageView color_image_view) = 0;

  FrameGraphRenderPass() = default;
  virtual ~FrameGraphRenderPass() = default;

  FrameGraphRenderPass(const FrameGraphRenderPass&) = delete;
  FrameGraphRenderPass& operator=(const FrameGraphRenderPass&) = delete;
};

} // namespace charlie

#endif // CHARLIE3D_RENDER_PASS_HPP
