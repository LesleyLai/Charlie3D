#ifndef CHARLIE3D_TEXTURES_HPP
#define CHARLIE3D_TEXTURES_HPP

#include <vulkan/vulkan_core.h>

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/image.hpp"
#include "uploader.hpp"

namespace charlie {

static constexpr uint32_t max_bindless_texture_count = 1024;
static constexpr uint32_t bindless_texture_binding = 10;

struct Texture {
  VkImage image = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
};

// Managers for textures in the scene
class TextureManager {
  vkh::Context& context_;
  UploadContext& upload_context_;
  VkSampler default_sampler_;

  u32 default_white_texture_index_;
  u32 default_normal_texture_index_;

  VkDescriptorSetLayout bindless_texture_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool bindless_texture_descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet bindless_texture_descriptor_set_ = VK_NULL_HANDLE;

  std::vector<vkh::AllocatedImage> images_;
  std::vector<Texture> textures_;
  struct TextureUpdate {
    uint32_t index = 0xdeadbeef; // Index
  };
  std::vector<TextureUpdate> textures_to_update_;

public:
  TextureManager(vkh::Context& context, UploadContext& upload_context, VkSampler default_sampler);
  ~TextureManager();

  // Add a texture and returns its index
  [[nodiscard]] auto add_texture(Texture texture) -> u32;

  [[nodiscard]] auto upload_image(const charlie::CPUImage& cpu_image,
                                  const ImageUploadInfo& upload_info) -> VkImage;

  // Returns the bindless texture descriptor set layout
  [[nodiscard]] auto descriptor_set_layout() const -> VkDescriptorSetLayout
  {
    return bindless_texture_set_layout_;
  }

  // Returns the bindless texture descriptor set
  [[nodiscard]] auto descriptor_set() const -> VkDescriptorSet
  {
    return bindless_texture_descriptor_set_;
  }

  [[nodiscard]] auto default_white_texture_index() const -> u32
  {
    return default_white_texture_index_;
  }

  [[nodiscard]] auto default_normal_texture_index() const -> u32
  {
    return default_normal_texture_index_;
  }

  // upload queued textures to the GPU
  void update();

  TextureManager(const TextureManager&) = delete;
  auto operator=(const TextureManager&) & -> TextureManager& = delete;
  TextureManager(TextureManager&&) noexcept = delete;
  auto operator=(TextureManager&&) & noexcept -> TextureManager& = delete;
};

} // namespace charlie

#endif // CHARLIE3D_TEXTURES_HPP
