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

constexpr unsigned int frame_overlap = 2;
struct FrameData {
  VkSemaphore present_semaphore{}, render_semaphore{};
  VkFence render_fence{};

  VkCommandPool command_pool{};
  VkCommandBuffer main_command_buffer{};

  vkh::Buffer camera_buffer{};
  VkDescriptorSet global_descriptor_set{};
};

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
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

  auto create_material(VkPipeline pipeline, VkPipelineLayout layout,
                       std::string name) -> Material&;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_material(const std::string& name) -> Material*;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_mesh(const std::string& name) -> Mesh*;
  void add_object(RenderObject object);

  // our draw function
  void draw_objects(VkCommandBuffer cmd, std::span<RenderObject> objects);

  [[nodiscard]] auto frame_number() const -> std::size_t
  {
    return frame_number_;
  }

  [[nodiscard]] auto current_frame() -> FrameData&
  {
    return frames_[frame_number_ % frame_overlap];
  }

private:
  Window* window_ = nullptr;
  vkh::Context context_;
  VkQueue graphics_queue_{};
  std::uint32_t graphics_queue_family_index{};

  vkh::Swapchain swapchain_;

  VkFormat depth_format_{};
  vkh::Image depth_image_;
  VkImageView depth_image_view_ = {};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};

  std::size_t frame_number_ = 0;
  FrameData frames_[frame_overlap];

  VkDescriptorSetLayout global_descriptor_set_layout_ = {};
  VkDescriptorPool descriptor_pool_ = {};

  VkPipelineLayout mesh_pipeline_layout_ = {};
  VkPipeline default_pipeline_ = {};

  std::unordered_map<std::string, Material> materials_;
  std::unordered_map<std::string, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
};

} // namespace charlie
