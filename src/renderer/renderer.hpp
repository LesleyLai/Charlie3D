#pragma once

#include "../asset/cpu_mesh.hpp"
#include "../utils/prelude.hpp"
#include "../window/input_handler.hpp"
#include "../window/window.hpp"
#include "vulkan_helpers/buffer.hpp"
#include "vulkan_helpers/context.hpp"
#include "vulkan_helpers/image.hpp"
#include "vulkan_helpers/swapchain.hpp"

#include <beyond/container/slot_map.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/utils/function_ref.hpp>

#include "../asset/cpu_scene.hpp"
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
  VkImage image = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
};

struct Material {
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

struct RenderObject {
  MeshHandle mesh;
  MaterialHandle material;
  Mat4 model_matrix;
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
  Vec4 sunlight_direction = {0, 1, 0, 1}; // w is used for ambient strength
  Vec4 sunlight_color = {1, 1, 1, 5};     // w for intensity
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

  [[nodiscard]] auto scene() const -> const Scene& { return *scene_; }

  void set_scene(std::unique_ptr<Scene> scene) { scene_ = std::move(scene); }

  // our draw function
  void draw_scene(VkCommandBuffer cmd, const charlie::Camera& camera);

  [[nodiscard]] auto frame_number() const -> usize { return frame_number_; }

  [[nodiscard]] auto resolution() const noexcept -> Resolution { return resolution_; }

  [[nodiscard]] auto current_frame() noexcept -> FrameData&
  {
    return frames_[frame_number_ % frame_overlap];
  }

  [[nodiscard]] auto context() noexcept -> vkh::Context& { return context_; }

  [[nodiscard]] auto upload_context() noexcept -> UploadContext& { return upload_context_; }

  [[nodiscard]] auto upload_mesh_data(const CPUMesh& cpu_mesh) -> MeshHandle;

  struct ImageUploadInfo {
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
  };

  auto upload_image(const charlie::CPUImage& cpu_image, const ImageUploadInfo& upload_info = {})
      -> VkImage;

  // Returns texture index
  auto add_texture(Texture texture) -> uint32_t;
  [[nodiscard]] auto texture_count() const -> uint32_t
  {
    return beyond::narrow<uint32_t>(textures_.size());
  }

  [[nodiscard]] auto create_material(const CPUMaterial& material) -> MaterialHandle;

  [[nodiscard]] auto scene_parameters() noexcept -> GPUSceneParameters&
  {
    return scene_parameters_;
  }

  uint32_t default_albedo_texture_index = static_cast<uint32_t>(~0);
  uint32_t default_normal_texture_index = static_cast<uint32_t>(~0);

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

  usize frame_number_ = 0;
  FrameData frames_[frame_overlap];

  std::unique_ptr<vkh::DescriptorAllocator> descriptor_allocator_;
  std::unique_ptr<vkh::DescriptorLayoutCache> descriptor_layout_cache_;
  VkDescriptorSetLayout global_descriptor_set_layout_ = {};
  VkDescriptorSetLayout object_descriptor_set_layout_ = {};
  VkDescriptorSetLayout material_set_layout_ = {};

  std::unique_ptr<class ShaderCompiler> shader_compiler_;

  VkPipelineLayout mesh_pipeline_layout_ = {};
  VkPipeline mesh_pipeline_ = {};

  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  beyond::SlotMap<MaterialHandle, Material> materials_;

  std::vector<vkh::AllocatedImage> images_;
  VkSampler sampler_ = VK_NULL_HANDLE;
  std::vector<Texture> textures_;

  std::vector<RenderObject> render_objects_;

  std::unique_ptr<Scene> scene_;
  GPUSceneParameters scene_parameters_;
  vkh::AllocatedBuffer scene_parameter_buffer_;

  std::unique_ptr<FrameGraphRenderPass> imgui_render_pass_ = nullptr;

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
  void init_sampler();
  void init_default_texture();

  void resize();

  void on_input_event(const Event& event, const InputStates& states) override;

  void present(u32& swapchain_image_index);
};

} // namespace charlie
