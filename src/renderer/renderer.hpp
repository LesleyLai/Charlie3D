#pragma once

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
#include "uploader.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace vkh {

struct Texture {
  Image image;
  VkImageView image_view = {};
};

class DescriptorAllocator;
class DescriptorLayoutCache;
class DescriptorBuilder;

} // namespace vkh

namespace tracy {

class VkCtx;

}

namespace charlie {

struct MeshHandle : beyond::GenerationalHandle<MeshHandle, uint32_t, 16> {
  using GenerationalHandle::GenerationalHandle;
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

  vkh::Buffer camera_buffer{};
  VkDescriptorSet global_descriptor_set{};

  vkh::Buffer object_buffer{};
  VkDescriptorSet object_descriptor_set{};

  vkh::Buffer indirect_buffer{};

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
  ~Renderer();
  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) & -> Renderer& = delete;
  Renderer(Renderer&&) noexcept = delete;
  auto operator=(Renderer&&) & noexcept -> Renderer& = delete;

  void render(const charlie::Camera& camera);

  auto create_material(VkPipeline pipeline, VkPipelineLayout layout, std::string name) -> Material&;

  // returns nullptr if it can't be found
  [[nodiscard]] auto get_material(const std::string& name) -> Material*;

  void add_object(RenderObject object);

  // our draw function
  void draw_objects(VkCommandBuffer cmd, std::span<RenderObject> objects,
                    const charlie::Camera& camera);

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

  [[nodiscard]] auto upload_mesh_data(const char* mesh_name, const CPUMesh& cpu_mesh) -> MeshHandle;

private:
  Window* window_ = nullptr;
  Resolution resolution_;
  vkh::Context context_;
  VkQueue transfer_queue_{};
  VkQueue graphics_queue_{};

  vkh::Swapchain swapchain_;

  UploadContext upload_context_;

  VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;
  vkh::Image depth_image_;
  VkImageView depth_image_view_ = {};

  std::size_t frame_number_ = 0;
  FrameData frames_[frame_overlap];

  std::unique_ptr<vkh::DescriptorAllocator> descriptor_allocator_ = nullptr;
  std::unique_ptr<vkh::DescriptorLayoutCache> descriptor_layout_cache_ = nullptr;
  VkDescriptorSetLayout global_descriptor_set_layout_ = {};
  VkDescriptorSetLayout object_descriptor_set_layout_ = {};
  VkDescriptorSetLayout single_texture_set_layout_ = {};

  VkPipelineLayout mesh_pipeline_layout_ = {};
  VkPipeline mesh_pipeline_ = {};

  std::unordered_map<std::string, Material> materials_;
  beyond::SlotMap<MeshHandle, Mesh> meshes_;
  std::vector<RenderObject> render_objects_;

  VkSampler blocky_sampler_ = VK_NULL_HANDLE;
  vkh::Texture texture_;

  std::unique_ptr<FrameGraphRenderPass> imgui_render_pass_ = nullptr;

  void init_frame_data();
  void init_depth_image();
  void init_descriptors();
  void init_pipelines();
  void init_texture();

  void resize();

  void on_input_event(const Event& event, const InputStates& states) override;

  auto upload_buffer(std::size_t size, const void* data, VkBufferUsageFlags usage,
                     const char* debug_name = "") -> vkh::Expected<vkh::Buffer>;

  template <class Container>
  auto upload_buffer(const Container& buffer, VkBufferUsageFlags usage, const char* debug_name = "")
      -> vkh::Expected<vkh::Buffer>
    requires(std::contiguous_iterator<typename Container::iterator>)
  {
    return upload_buffer(buffer.size() * sizeof(typename Container::value_type), buffer.data(),
                         usage, debug_name);
  }

  void present(uint32_t& swapchain_image_index);
};

} // namespace charlie
