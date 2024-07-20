#pragma once

#include "../asset_handling/cpu_mesh.hpp"
#include "../utils/prelude.hpp"
#include "../vulkan_helpers/buffer.hpp"
#include "../vulkan_helpers/context.hpp"
#include "../vulkan_helpers/image.hpp"
#include "../vulkan_helpers/swapchain.hpp"
#include "../window/input_handler.hpp"
#include "../window/window.hpp"

#include "shadow_map_renderer.hpp"

#include <beyond/container/slot_map.hpp>
#include <beyond/math/matrix.hpp>
#include <beyond/utils/function_ref.hpp>
#include <beyond/utils/ref.hpp>

#include "../asset_handling/cpu_scene.hpp"
#include "deletion_queue.hpp"
#include "mesh.hpp"
#include "pipeline_manager.hpp"
#include "render_pass.hpp"
#include "sampler_cache.hpp"
#include "scene.hpp"
#include "textures.hpp"
#include "uploader.hpp"

#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace tracy {

class VkCtx;

}

namespace charlie {

// TODO: find better way to allocate object buffer
constexpr beyond::usize max_object_count = 10000000;

class DescriptorAllocator;
class DescriptorLayoutCache;

struct Material {
  Vec4 base_color_factor = Vec4(1, 1, 1, 1);
  u32 albedo_texture_index = 0xdeadbeef;
  u32 normal_texture_index = 0xdeadbeef;
  u32 metallic_roughness_texture_index = 0xdeadbeef;
  u32 occlusion_texture_index = 0xdeadbeef;

  Vec3 emissive_factor;
  u32 emissive_texture_index = 0xdeafbeef;

  f32 metallic_factor = 1.0f;
  f32 roughness_factor = 1.0f;
  f32 alpha_cutoff = 1.0f;
  f32 _padding = 0.0f;
};

struct RenderObject {
  const SubMesh* submesh = nullptr;
  u32 node_index = static_cast<u32>(~0); // Index of the node in scene graph. Used to look up
                                         // informations such as the transformations
};

constexpr unsigned int frame_overlap = 2;
struct FrameData {
  VkSemaphore present_semaphore{}, render_semaphore{};
  VkFence render_fence{};

  VkCommandPool command_pool{};
  VkCommandBuffer main_command_buffer{};

  vkh::AllocatedBuffer camera_buffer{};
  VkDescriptorSet global_descriptor_set{};

  vkh::AllocatedBuffer transform_buffer{}; // model matrix for each scene graph node
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

  // Shadow mode
  // 0: no shadow
  // 1: shadow map
  // 2: shadow map pcf
  // 3: shadow map PCSS
  u32 sunlight_shadow_mode = 3;
};

struct MeshPushConstant {
  VkDeviceAddress position_buffer_address = 0;
  VkDeviceAddress vertex_buffer_address = 0;
};

// Buffers for mesh data
struct MeshBuffers {
  vkh::AllocatedBuffer position_buffer;
  vkh::AllocatedBuffer vertex_buffer;
  vkh::AllocatedBuffer index_buffer;
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

  void set_scene(std::unique_ptr<Scene> scene);

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

  /*
   * Resource management functions
   */
  /**
   * Upload the vertex/index buffers of a mesh to the GPU
   *
   * The name is the debug name used for renderdoc
   */
  [[nodiscard]] auto upload_mesh_buffer(const CPUMeshBuffers& buffers,
                                        std::string_view name) -> MeshBuffers;
  [[nodiscard]] auto add_mesh(const CPUMesh& mesh) -> MeshHandle;

  // Uploads an image to GPU
  [[nodiscard]] auto upload_image(const charlie::CPUImage& cpu_image,
                                  const ImageUploadInfo& upload_info = {}) -> VkImage;

  // Adds a texture and returns its index
  [[nodiscard]] auto add_texture(Texture texture) -> u32;
  [[nodiscard]] auto add_material(const CPUMaterial& material_info) -> u32;
  void upload_materials();

  void resize();

  void draw_gui_lighting_window();

  VkDescriptorSetLayout global_descriptor_set_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayout object_descriptor_set_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayout material_descriptor_set_layout = VK_NULL_HANDLE;

  [[nodiscard]] auto pipeline_manager() -> PipelineManager& { return *pipeline_manager_; }

  [[nodiscard]] auto draw_solid_objects() const -> std::span<const RenderObject>
  {
    return draws_solid_objects_;
  }

  MeshBuffers scene_mesh_buffers;

private:
  Window* window_ = nullptr;
  Resolution resolution_;
  vkh::Context context_;
  VkQueue graphics_queue_{};

  std::unique_ptr<SamplerCache> sampler_cache_;

  vkh::Swapchain swapchain_;
  UploadContext upload_context_;

  static constexpr VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
  vkh::AllocatedImage depth_image_;
  VkImageView depth_image_view_ = {};

  usize frame_number_ = 0;
  FrameData frames_[frame_overlap];
  DeletionQueue frame_deletion_queue_[frame_overlap];

  std::unique_ptr<charlie::DescriptorAllocator> descriptor_allocator_;
  std::unique_ptr<charlie::DescriptorLayoutCache> descriptor_layout_cache_;

  std::unique_ptr<class ShaderCompiler> shader_compiler_;
  std::unique_ptr<PipelineManager> pipeline_manager_;

  std::unique_ptr<ShadowMapRenderer> shadow_map_renderer_;

  VkPipelineLayout mesh_pipeline_layout_ = VK_NULL_HANDLE;
  GraphicsPipelineHandle mesh_pipeline_;
  GraphicsPipelineHandle mesh_pipeline_transparent_;

  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  std::vector<Material> materials_;
  std::vector<AlphaMode> material_alpha_modes_;

  vkh::AllocatedBuffer material_buffer_;
  VkDescriptorSet material_descriptor_set_ = VK_NULL_HANDLE;

  std::unique_ptr<TextureManager> textures_;

  std::vector<RenderObject> draws_solid_objects_;
  std::vector<RenderObject> draws_transparent_objects_;

  std::unique_ptr<Scene> scene_;

  GPUSceneParameters scene_parameters_;
  vkh::AllocatedBuffer scene_parameter_buffer_;

  std::unique_ptr<FrameGraphRenderPass> imgui_render_pass_ = nullptr;

  void update(const charlie::Camera& camera);

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
  void init_mesh_pipeline();

  void on_input_event(const Event& event, const InputStates& states);

  void present(beyond::Ref<u32> swapchain_image_index);
};

} // namespace charlie
