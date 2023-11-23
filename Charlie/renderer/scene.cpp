#include "scene.hpp"

#include "../asset/gltf_loader.hpp"
#include "../asset/obj_loader.hpp"

#include "../utils/configuration.hpp"

#include "../vulkan_helpers/initializers.hpp"

#include "renderer.hpp"

#include <tracy/Tracy.hpp>

namespace charlie {

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene
{
  ZoneScoped;

  std::filesystem::path file_path = filename;
  if (file_path.is_relative()) {
    const auto& assets_path =
        Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
    file_path = assets_path / file_path;
  }

  CPUScene cpu_scene = [&]() {
    if (file_path.extension() == ".obj") {
      return load_obj(file_path);
    } else if (file_path.extension() == ".gltf") {
      return load_gltf(file_path);
    } else {
      beyond::panic("Unknown scene format!");
    }
  }();

  std::vector<MeshHandle> mesh_storage;
  mesh_storage.reserve(cpu_scene.meshes.size());
  std::ranges::transform(cpu_scene.meshes, std::back_inserter(mesh_storage),
                         [&](const CPUMesh& mesh) { return renderer.upload_mesh_data(mesh); });

  std::vector<VkImage> images(cpu_scene.images.size(), VK_NULL_HANDLE);
  std::ranges::transform(cpu_scene.images, images.begin(), [&](const CPUImage& cpu_image) {
    return renderer.upload_image(cpu_image);
  });

  // A map from local index to resource index
  std::vector<uint32_t> texture_indices_map(cpu_scene.textures.size());
  std::ranges::transform(
      cpu_scene.textures, texture_indices_map.begin(), [&](const CPUTexture& cpu_texture) {
        VkImage image = images.at(cpu_texture.image_index);
        VkImageView image_view =
            vkh::create_image_view(
                renderer.context(),
                {.image = image,
                 .format = VK_FORMAT_R8G8B8A8_SRGB,
                 .subresource_range =
                     vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT}})
                .value();

        return renderer.add_texture(Texture{.image = image, .image_view = image_view});
      });
  const auto lookup_texture_index = [&](u32 local_index) {
    return texture_indices_map.at(local_index);
  };

  for (const CPUMaterial& material : cpu_scene.materials) {
    [[maybe_unused]] u32 material_index = renderer.add_material(CPUMaterial{
        .base_color_factor = material.base_color_factor,
        .albedo_texture_index = material.albedo_texture_index.map(lookup_texture_index),
        .normal_texture_index = material.normal_texture_index.map(lookup_texture_index),
        .occlusion_texture_index = material.occlusion_texture_index.map(lookup_texture_index)});
  }

  std::unordered_map<uint32_t, RenderComponent> render_components;
  for (u32 i = 0; i < cpu_scene.objects.size(); ++i) {
    const auto& object = cpu_scene.objects[i];
    if (object.mesh_index >= 0) {
      const auto mesh_handle = mesh_storage[narrow<u32>(object.mesh_index)];

      render_components.insert({i, RenderComponent{.mesh = mesh_handle}});
    }
  }
  renderer.upload_materials();

  auto local_transforms = std::move(cpu_scene.local_transforms);
  std::vector<beyond::Mat4> global_transforms(local_transforms);
  // TODO: generate global transform from local transform

  return Scene{
      .local_transforms = std::move(local_transforms),
      .global_transforms = std::move(global_transforms),
      .render_components = std::move(render_components),
  };
}

} // namespace charlie