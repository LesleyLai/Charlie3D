#pragma once

#include "../window/window.hpp"
#include "vulkan_helpers/buffer.hpp"
#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/swapchain.hpp"

#include <beyond/math/matrix.hpp>
#include <beyond/utils/function_ref.hpp>

#include "mesh.hpp"

#include <imgui.h>

#include <span>
#include <unordered_map>
#include <vector>

namespace vkh {

struct Image {
  VkImage image = {};
  VmaAllocation allocation = {};
};

struct Texture {
  Image image;
  VkImageView image_view = {};
};

} // namespace vkh

namespace charlie {

struct Material {
  VkPipeline pipeline = {};
  VkPipelineLayout pipeline_layout = {};
  VkDescriptorSet texture_set = {};
};

struct RenderObject {
  Mesh* mesh = nullptr;
  Material* material = nullptr;
  beyond::Mat4 model_matrix;
};

constexpr unsigned int frame_overlap = 2;
struct FrameData {
  VkSemaphore present_semaphore{}, render_semaphore{};
  VkFence render_fence{};

  VkCommandPool command_pool{};
  VkCommandBuffer main_command_buffer{};

  vkh::Buffer camera_buffer{};
  VkDescriptorSet global_descriptor_set{};

  vkh::Buffer object_buffer{};
  VkDescriptorSet object_descriptor_set{};
};

struct GPUObjectData {
  beyond::Mat4 model;
};

struct GPUCameraData {
  beyond::Mat4 view;
  beyond::Mat4 proj;
  beyond::Mat4 view_proj;
};

struct UploadContext {
  VkFence fence = {};
  VkCommandPool command_pool = {};
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

  [[nodiscard]] auto current_frame() noexcept -> FrameData&
  {
    return frames_[frame_number_ % frame_overlap];
  }

  void immediate_submit(beyond::function_ref<void(VkCommandBuffer)> function);

  [[nodiscard]] auto context() noexcept -> vkh::Context&
  {
    return context_;
  }

private:
  Window* window_ = nullptr;
  vkh::Context context_;
  VkQueue graphics_queue_{};
  std::uint32_t graphics_queue_family_index{};

  vkh::Swapchain swapchain_;

  UploadContext upload_context_;

  VkFormat depth_format_{};
  vkh::Image depth_image_;
  VkImageView depth_image_view_ = {};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};

  std::size_t frame_number_ = 0;
  FrameData frames_[frame_overlap];

  VkDescriptorSetLayout global_descriptor_set_layout_ = {};
  VkDescriptorSetLayout object_descriptor_set_layout_ = {};
  VkDescriptorSetLayout single_texture_set_layout_ = {};
  VkDescriptorPool descriptor_pool_ = {};

  VkPipelineLayout mesh_pipeline_layout_ = {};
  VkPipeline default_pipeline_ = {};

  std::unordered_map<std::string, Material> materials_;
  std::unordered_map<std::string, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  VkSampler blocky_sampler_;
  vkh::Texture texture_;

  VkDescriptorPool imgui_pool_;

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
  void init_upload_context();
  void init_imgui();

  [[nodiscard]] auto load_mesh(vkh::Context& context, const char* filename)
      -> Mesh;

  [[nodiscard]] auto upload_mesh_data(vkh::Context& context,
                                      std::span<const Vertex> vertices,
                                      std::span<const std::uint32_t> indices)
      -> Mesh;

  auto upload_buffer(std::size_t gpu_buffer, const void* data,
                     VkBufferUsageFlags usage) -> vkh::Expected<vkh::Buffer>;
};

} // namespace charlie
