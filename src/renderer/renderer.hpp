#pragma once

#include "../window/window.hpp"
#include "vulkan_helpers/buffer.hpp"
#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/swapchain.hpp"

#include "mesh.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace vkh {

struct Image {
  VkImage image = {};
  VmaAllocation allocation = {};
};

} // namespace vkh

namespace charlie {

struct Material {
  VkPipeline pipeline = {};
  VkPipelineLayout pipeline_layout = {};
};

struct RenderObject {
  Mesh* mesh = nullptr;
  Material* material = nullptr;
  beyond::Mat4 transform_matrix;
};

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

  VkFormat depth_format_{};
  vkh::Image depth_image_;
  VkImageView depth_image_view_ = {};

  VkCommandPool command_pool_{};
  VkCommandBuffer main_command_buffer_{};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};

  VkSemaphore present_semaphore_{}, render_semaphore_{};
  VkFence render_fence_{};

  std::size_t frame_number_ = 0;

  VkPipelineLayout default_pipeline_layout_ = {};
  VkPipeline default_pipeline_ = {};

  std::unordered_map<std::string, Material> materials_;
  std::unordered_map<std::string, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  auto create_material(VkPipeline pipeline, VkPipelineLayout layout,
                       std::string name) -> Material&;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_material(const std::string& name) -> Material*;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_mesh(const std::string& name) -> Mesh*;

  // our draw function
  void draw_objects(VkCommandBuffer cmd, std::span<RenderObject> objects);

  void init_sync_structures();
  void init_depth_image();
  void init_pipelines();
  void init_scene();
};

} // namespace charlie
