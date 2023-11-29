#pragma once

#include "../asset/cpu_mesh.hpp"
#include "../utils/prelude.hpp"
#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/image.hpp"
#include "../vulkan_helpers/swapchain.hpp"
#include "../window/input_handler.hpp"
#include "../window/window.hpp"

#include <beyond/container/slot_map.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/utils/function_ref.hpp>
#include <beyond/utils/ref.hpp>

#include "../asset/cpu_scene.hpp"
#include "deletion_queue.hpp"
#include "mesh.hpp"
#include "pipeline_manager.hpp"
#include "render_pass.hpp"
#include "scene.hpp"
#include "uploader.hpp"

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace tracy {

class VkCtx;

}

namespace charlie {

class DescriptorAllocator;
class DescriptorLayoutCache;

struct Texture {
  VkImage image = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
};

struct Material {
  Vec4 base_color_factor = Vec4(1, 1, 1, 1);
  u32 albedo_texture_index = 0xdeadbeef;
  u32 normal_texture_index = 0xdeadbeef;
  u32 metallic_roughness_texture_index = 0xdeadbeef;
  u32 occlusion_texture_index = 0xdeadbeef;
  f32 metallic_factor = 1.0f;
  f32 roughness_factor = 1.0f;
};

struct RenderObject {
  const SubMesh* submesh = nullptr;
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

  vkh::AllocatedBuffer model_matrix_buffer{};
  vkh::AllocatedBuffer material_index_buffer{};
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
  Vec4 sunlight_direction = {0, -1, -1, 0.1f}; // w is used for ambient strength
  Vec4 sunlight_color = {1, 1, 1, 5};          // w for sunlight intensity
  Mat4 sunlight_view_proj;
};

struct MeshPushConstant {
  VkDeviceAddress position_buffer_address = 0;
  VkDeviceAddress normal_buffer_address = 0;
  VkDeviceAddress tex_coord_buffer_address = 0;
  VkDeviceAddress tangent_buffer_address = 0;
};

class Renderer {
public:
  explicit Renderer(Window& window, InputHandler& input_handler);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) & -> Renderer& = delete;
  Renderer(Renderer&&) noexcept = delete;
  auto operator=(Renderer&&) & noexcept -> Renderer& = delete;

  void render(const charlie::Camera& camera);

  [[nodiscard]] auto scene() const -> const Scene& { return *scene_; }

  void set_scene(std::unique_ptr<Scene> scene) { scene_ = std::move(scene); }

  void draw_shadow(VkCommandBuffer cmd);
  void draw_scene(VkCommandBuffer cmd, VkImageView current_swapchain_image_view);

  [[nodiscard]] auto resolution() const noexcept -> Resolution { return resolution_; }

  [[nodiscard]] BEYOND_FORCE_INLINE auto current_frame_index() const noexcept -> usize
  {
    return frame_number_ % frame_overlap;
  }

  [[nodiscard]] auto current_frame() noexcept -> FrameData&
  {
    return frames_[current_frame_index()];
  }

  [[nodiscard]] auto current_frame_deletion_queue() noexcept -> DeletionQueue&
  {
    return frame_deletion_queue_[current_frame_index()];
  }

  [[nodiscard]] auto context() noexcept -> vkh::Context& { return context_; }

  [[nodiscard]] auto upload_mesh_data(const CPUMesh& cpu_mesh) -> MeshHandle;

  struct ImageUploadInfo {
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    u32 mip_levels = 1; // Generate mipmaps if mip_level > 1
  };

  auto upload_image(const charlie::CPUImage& cpu_image, const ImageUploadInfo& upload_info = {})
      -> VkImage;

  // Returns texture index
  auto add_texture(Texture texture) -> u32;

  [[nodiscard]] auto add_material(const CPUMaterial& material_info) -> u32;
  void upload_materials();

  [[nodiscard]] auto scene_parameters() noexcept -> GPUSceneParameters&
  {
    return scene_parameters_;
  }

  void resize();

  u32 default_albedo_texture_index = static_cast<u32>(~0);
  u32 default_normal_texture_index = static_cast<u32>(~0);

  bool enable_shadow_mapping = true;

private:
  Window* window_ = nullptr;
  Resolution resolution_;
  vkh::Context context_;
  VkQueue graphics_queue_{};

  vkh::Swapchain swapchain_;

  UploadContext upload_context_;

  static constexpr VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;
  vkh::AllocatedImage depth_image_;
  VkImageView depth_image_view_ = {};

  static constexpr uint32_t shadow_map_width_ = 4096;
  static constexpr uint32_t shadow_map_height_ = 4096;
  vkh::AllocatedImage shadow_map_images_[frame_overlap];
  VkImageView shadow_map_image_views_[frame_overlap];
  VkSampler shadow_map_sampler_ = VK_NULL_HANDLE;

  usize frame_number_ = 0;
  FrameData frames_[frame_overlap];
  DeletionQueue frame_deletion_queue_[frame_overlap];

  std::unique_ptr<charlie::DescriptorAllocator> descriptor_allocator_;
  std::unique_ptr<charlie::DescriptorLayoutCache> descriptor_layout_cache_;
  VkDescriptorSetLayout global_descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout object_descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout material_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout bindless_texture_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool bindless_texture_descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet bindless_texture_descriptor_set_ = VK_NULL_HANDLE;

  std::unique_ptr<class ShaderCompiler> shader_compiler_;

  std::unique_ptr<PipelineManager> pipeline_manager_;

  VkPipelineLayout shadow_map_pipeline_layout_ = VK_NULL_HANDLE;
  GraphicsPipelineHandle shadow_map_pipeline_;

  VkPipelineLayout mesh_pipeline_layout_ = VK_NULL_HANDLE;
  GraphicsPipelineHandle mesh_pipeline_without_shadow_;
  GraphicsPipelineHandle mesh_pipeline_;

  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  std::vector<Material> materials_;
  vkh::AllocatedBuffer material_buffer_;
  VkDescriptorSet material_descriptor_set_ = VK_NULL_HANDLE;

  std::vector<vkh::AllocatedImage> images_;
  VkSampler default_sampler_ = VK_NULL_HANDLE;
  std::vector<Texture> textures_;
  struct TextureUpdate {
    uint32_t index = 0xdeadbeef; // Index
  };
  std::vector<TextureUpdate> textures_to_update_;

  std::vector<RenderObject> draws_;

  std::unique_ptr<Scene> scene_;
  GPUSceneParameters scene_parameters_;
  vkh::AllocatedBuffer scene_parameter_buffer_;

  std::unique_ptr<FrameGraphRenderPass> imgui_render_pass_ = nullptr;

  void init_frame_data();
  void init_depth_image();
  void init_shadow_map();
  void init_descriptors();
  void init_pipelines();
  void init_shadow_pipeline();
  void init_mesh_pipeline();
  void init_sampler();
  void init_default_texture();

  void on_input_event(const Event& event, const InputStates& states);

  void update_textures();
  void present(beyond::Ref<u32> swapchain_image_index);
};

} // namespace charlie
