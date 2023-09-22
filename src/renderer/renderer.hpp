#pragma once

#include "../asset//cpu_mesh.hpp"
#include "../window/input_handler.hpp"
#include "../window/window.hpp"
#include "vulkan_helpers/buffer.hpp"
#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/image.hpp"
#include "vulkan_helpers/swapchain.hpp"

#include <beyond/container/slot_map.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/utils/function_ref.hpp>

#include "mesh.hpp"
#include "render_pass.hpp"
#include "scene.hpp"
#include "uploader.hpp"

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace vkh {

class DescriptorAllocator;
class DescriptorLayoutCache;
class DescriptorBuilder;

} // namespace vkh

namespace tracy {

class VkCtx;

}

namespace charlie {

struct Texture {
  vkh::AllocatedImage image;
  VkImageView image_view = {};
};

struct RenderObject {
  MeshHandle mesh;
  beyond::Mat4 model_matrix;
};

constexpr unsigned int frame_overlap = 2;
struct FrameData {
  VkSemaphore present_semaphore{}, render_semaphore{};
  VkFence render_fence{};

  VkCommandPool command_pool{};
  VkCommandBuffer main_command_buffer{};

  vkh::AllocatedBuffer camera_buffer{};
  VkDescriptorSet global_descriptor_set{};

  vkh::AllocatedBuffer object_buffer{};
  VkDescriptorSet object_descriptor_set{};

  vkh::AllocatedBuffer indirect_buffer{};

  tracy::VkCtx* tracy_vk_ctx = nullptr;
};

[[nodiscard]] constexpr auto to_extent2d(Resolution res)
{
  return VkExtent2D{.width = res.width, .height = res.height};
}

class Camera;

struct GPUSceneParameters {
  beyond::Vec4 sunlight_direction = {0, 1, 0, 0}; // w unused
  beyond::Vec4 sunlight_color = {1, 1, 1, 1};     // w for intensity
};

class Renderer : public InputListener {
public:
  explicit Renderer(Window& window);
  ~Renderer() override;
  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) & -> Renderer& = delete;
  Renderer(Renderer&&) noexcept = delete;
  auto operator=(Renderer&&) & noexcept -> Renderer& = delete;

  void render(const charlie::Camera& camera);

  auto set_scene(std::unique_ptr<const Scene> scene) -> const Scene&;

  // our draw function
  void draw_scene(VkCommandBuffer cmd, const charlie::Camera& camera);

  [[nodiscard]] auto frame_number() const -> std::size_t
  {
    return frame_number_;
  }

  [[nodiscard]] auto resolution() const noexcept -> Resolution
  {
    return resolution_;
  }

  [[nodiscard]] auto current_frame() noexcept -> FrameData&
  {
    return frames_[frame_number_ % frame_overlap];
  }

  [[nodiscard]] auto context() noexcept -> vkh::Context&
  {
    return context_;
  }

  [[nodiscard]] auto upload_context() noexcept -> UploadContext&
  {
    return upload_context_;
  }

  [[nodiscard]] auto upload_mesh_data(const CPUMesh& cpu_mesh) -> MeshHandle;

  [[nodiscard]] auto scene_parameters() noexcept -> GPUSceneParameters&
  {
    return scene_parameters_;
  }

private:
  Window* window_ = nullptr;
  Resolution resolution_;
  vkh::Context context_;
  VkQueue transfer_queue_{};
  VkQueue graphics_queue_{};

  vkh::Swapchain swapchain_;

  UploadContext upload_context_;

  VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;
  vkh::AllocatedImage depth_image_;
  VkImageView depth_image_view_ = {};

  std::size_t frame_number_ = 0;
  FrameData frames_[frame_overlap];

  std::unique_ptr<vkh::DescriptorAllocator> descriptor_allocator_ = nullptr;
  std::unique_ptr<vkh::DescriptorLayoutCache> descriptor_layout_cache_ = nullptr;
  VkDescriptorSetLayout global_descriptor_set_layout_ = {};
  VkDescriptorSetLayout object_descriptor_set_layout_ = {};
  VkDescriptorSetLayout single_texture_set_layout_ = {};

  std::unique_ptr<class ShaderCompiler> shader_compiler_ = nullptr;

  VkPipelineLayout mesh_pipeline_layout_ = {};
  VkPipeline mesh_pipeline_ = {};

  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  std::unique_ptr<const Scene> scene_;
  GPUSceneParameters scene_parameters_;
  vkh::AllocatedBuffer scene_parameter_buffer_;

  VkSampler blocky_sampler_ = VK_NULL_HANDLE;
  Texture texture_;
  VkDescriptorSet texture_set_ = {};

  std::unique_ptr<FrameGraphRenderPass> imgui_render_pass_ = nullptr;

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
  void init_texture();

  void resize();

  void on_input_event(const Event& event, const InputStates& states) override;

  void present(uint32_t& swapchain_image_index);
};

} // namespace charlie
