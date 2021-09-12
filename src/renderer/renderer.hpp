#pragma once

#include "../window/window.hpp"
#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/swapchain.hpp"

#include <vector>

namespace charlie {

class Renderer {
public:
  explicit Renderer(Window& window);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) & -> Renderer& = delete;
  Renderer(Renderer&&) noexcept = delete;
  auto operator=(Renderer&&) & noexcept -> Renderer& = delete;

  void render();

private:
  Window* window_ = nullptr;
  vkh::Context context_;
  VkQueue graphics_queue_{};
  std::uint32_t graphics_queue_family_index{};

  vkh::Swapchain swapchain_;

  VkCommandPool command_pool_{};
  VkCommandBuffer main_command_buffer_{};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};

  VkSemaphore present_semaphore_{}, render_semaphore_{};
  VkFence render_fence_{};

  std::size_t frame_number_ = 0;

  VkPipelineLayout triangle_pipeline_layout_{};
  VkPipeline triangle_pipeline_{};

  void init_sync_structures();
  void init_pipelines();
};

} // namespace charlie
