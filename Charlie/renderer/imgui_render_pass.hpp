#ifndef CHARLIE3D_IMGUI_RENDER_PASS_HPP
#define CHARLIE3D_IMGUI_RENDER_PASS_HPP

#include <vulkan/vulkan.h>

#include "render_pass.hpp"

struct SDL_Window;

namespace charlie {

class Renderer;

class ImguiRenderPass : public FrameGraphRenderPass {
  Renderer* renderer_ = nullptr;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

public:
  explicit ImguiRenderPass(Renderer& renderer, SDL_Window* window,
                           VkFormat color_attachment_format);
  ~ImguiRenderPass() override;

  void pre_render() override;
  void render(VkCommandBuffer cmd, VkImageView color_image_view) override;
};

} // namespace charlie

#endif // CHARLIE3D_IMGUI_RENDER_PASS_HPP
