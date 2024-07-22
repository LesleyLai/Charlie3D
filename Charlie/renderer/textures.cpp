#include "textures.hpp"
#include "../asset_handling/cpu_image.hpp"
#include "../vulkan_helpers/initializers.hpp"
#include "uploader.hpp"

#include <beyond/container/static_vector.hpp>

#include <tracy/Tracy.hpp>

namespace charlie {

TextureManager::TextureManager(vkh::Context& context, UploadContext& upload_context,
                               VkSampler default_sampler)
    : context_{context}, upload_context_{upload_context}, default_sampler_{default_sampler}
{
  static constexpr VkDescriptorSetLayoutBinding texture_bindings[] = {
      // Image sampler binding
      {.binding = bindless_texture_binding,
       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = max_bindless_texture_count,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

  static constexpr VkDescriptorBindingFlags bindless_flags =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; // VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
  static constexpr VkDescriptorBindingFlags binding_flags[] = {bindless_flags};

  VkDescriptorSetLayoutBindingFlagsCreateInfo layout_binding_flags_create_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .bindingCount = 1,
      .pBindingFlags = binding_flags,
  };

  {
    vkh::DescriptorSetLayoutCreateInfo bindless_texture_descriptor_set_layout_create_info{
        .p_next = &layout_binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindings = texture_bindings,
        .debug_name = "Material Descriptor Set Layout",
    };

    static constexpr VkDescriptorPoolSize pool_sizes_bindless[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_bindless_texture_count},
    };
    bindless_texture_set_layout_ = vkh::create_descriptor_set_layout(
                                       context_, bindless_texture_descriptor_set_layout_create_info)
                                       .value();
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = max_bindless_texture_count * beyond::size(pool_sizes_bindless),
        .poolSizeCount = beyond::size(pool_sizes_bindless),
        .pPoolSizes = pool_sizes_bindless,
    };
    VK_CHECK(vkCreateDescriptorPool(context_, &descriptor_pool_create_info, nullptr,
                                    &bindless_texture_descriptor_pool_));

    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = bindless_texture_descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &bindless_texture_set_layout_};

    VK_CHECK(vkAllocateDescriptorSets(context_, &alloc_info, &bindless_texture_descriptor_set_));
  }

  // Add default textures
  {
    CPUImage cpu_image{
        .name = "Default Albedo Texture Image",
        .width = 1,
        .height = 1,
        .components = 4,
        .data = std::make_unique_for_overwrite<uint8_t[]>(sizeof(uint8_t) * 4),
    };
    std::fill(cpu_image.data.get(), cpu_image.data.get() + 4, 255);

    const auto default_albedo_image =
        upload_image(cpu_image, {
                                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                                });

    VkImageView default_albedo_image_view =
        vkh::create_image_view(
            context_,
            {.image = default_albedo_image,
             .format = VK_FORMAT_R8G8B8A8_UNORM,
             .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
             .debug_name = "Default Albedo Texture Image View"})
            .value();

    default_white_texture_index_ = add_texture(
        Texture{.image = default_albedo_image, .image_view = default_albedo_image_view});

    CPUImage cpu_image2{
        .name = "Default Normal Texture Image",
        .width = 1,
        .height = 1,
        .components = 4,
        .data = std::make_unique_for_overwrite<uint8_t[]>(sizeof(uint8_t) * 4),
    };
    cpu_image2.data[0] = 127;
    cpu_image2.data[1] = 127;
    cpu_image2.data[2] = 255;
    cpu_image2.data[3] = 255;

    const auto default_normal_image =
        upload_image(cpu_image2, {
                                     .format = VK_FORMAT_R8G8B8A8_UNORM,
                                 });
    VkImageView default_normal_image_view =
        vkh::create_image_view(
            context_,
            {.image = default_normal_image,
             .format = VK_FORMAT_R8G8B8A8_UNORM,
             .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT},
             .debug_name = "Default Normal Texture Image View"})
            .value();

    default_normal_texture_index_ = add_texture(
        Texture{.image = default_normal_image, .image_view = default_normal_image_view});
  }
}

TextureManager::~TextureManager()
{
  for (auto texture : textures_) { vkDestroyImageView(context_, texture.image_view, nullptr); }
  for (auto image : images_) { vkh::destroy_image(context_, image); }
}

auto TextureManager::add_texture(Texture texture) -> u32
{
  if (texture.sampler == VK_NULL_HANDLE) { texture.sampler = default_sampler_; };

  textures_.push_back(texture);
  const u32 texture_index = narrow<u32>(textures_.size() - 1);

  textures_to_update_.push_back(TextureUpdate{
      .index = texture_index,
  });

  return texture_index;
}

[[nodiscard]] auto TextureManager::upload_image(const charlie::CPUImage& cpu_image,
                                                const ImageUploadInfo& upload_info) -> VkImage
{
  ZoneScoped;

  auto image = charlie::upload_image(context_, upload_context_, cpu_image, upload_info);

  return images_.emplace_back(image).image;
}

void TextureManager::update()
{
  ZoneScoped;

  beyond::StaticVector<VkDescriptorImageInfo, max_bindless_texture_count> image_infos;
  beyond::StaticVector<VkWriteDescriptorSet, max_bindless_texture_count> descriptor_writes;

  for (const TextureUpdate& texture_to_update : textures_to_update_) {
    const Texture& texture = textures_.at(texture_to_update.index);

    BEYOND_ENSURE(texture.sampler != nullptr);
    BEYOND_ENSURE(texture.image_view != nullptr);

    VkDescriptorImageInfo& image_info = image_infos.emplace_back(
        VkDescriptorImageInfo{.sampler = texture.sampler,
                              .imageView = texture.image_view,
                              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    descriptor_writes.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bindless_texture_descriptor_set_,
        .dstBinding = bindless_texture_binding,
        .dstArrayElement = texture_to_update.index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    });
  }
  textures_to_update_.clear();

  vkUpdateDescriptorSets(context_, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

} // namespace charlie
