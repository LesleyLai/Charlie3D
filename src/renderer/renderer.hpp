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

struct Material {
  VkPipeline pipeline = {};
  VkPipelineLayout pipeline_layout = {};
  VkDescriptorSet texture_set = {};
};

struct RenderObject {
  MeshHandle mesh;
  const Material* material = nullptr;
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

class Renderer : public InputListener {
public:
  explicit Renderer(Window& window);
  ~Renderer() override;
  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) & -> Renderer& = delete;
  Renderer(Renderer&&) noexcept = delete;
  auto operator=(Renderer&&) & noexcept -> Renderer& = delete;

  void render(const charlie::Camera& camera);

  auto create_material(VkPipeline pipeline, VkPipelineLayout layout, std::string name) -> Material&;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_material(const std::string& name) -> Material*;

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

  std::unordered_map<std::string, Material> materials_;
  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  std::unique_ptr<const Scene> scene_;

  VkSampler blocky_sampler_ = VK_NULL_HANDLE;
  Texture texture_;

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
