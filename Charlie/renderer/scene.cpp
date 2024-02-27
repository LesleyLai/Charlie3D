#include "scene.hpp"

#include "../asset/gltf_loader.hpp"
#include "../asset/obj_loader.hpp"

#include "../utils/asset_path.hpp"

#include "../vulkan_helpers/initializers.hpp"

#include "beyond/math/serial.hpp"

#include "renderer.hpp"

#include <tracy/Tracy.hpp>

namespace charlie {

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene
{
  ZoneScoped;

  std::filesystem::path file_path = filename;
  if (file_path.is_relative()) {
    const auto& assets_path = get_asset_path();
    file_path = assets_path / file_path;
  }

  CPUScene cpu_scene = [&]() {
    if (file_path.extension() == ".obj") {
      return load_obj(file_path);
    } else if (file_path.extension() == ".gltf" || file_path.extension() == ".glb") {
      return load_gltf(file_path);
    } else {
      beyond::panic("Unknown scene format!");
    }
  }();

  std::vector<MeshHandle> mesh_storage;
  {
    ZoneScopedN("Upload mesh");
    mesh_storage.reserve(cpu_scene.meshes.size());
    std::ranges::transform(cpu_scene.meshes, std::back_inserter(mesh_storage),
                           [&](const CPUMesh& mesh) { return renderer.upload_mesh_data(mesh); });
  }

  std::vector<VkImage> images(cpu_scene.images.size(), VK_NULL_HANDLE);
  std::vector<VkImageView> image_views(cpu_scene.images.size(), VK_NULL_HANDLE);
  {
    ZoneScopedN("Upload images");
    for (usize i = 0; i < cpu_scene.images.size(); ++i) {
      const CPUImage& cpu_image = cpu_scene.images[i];
      const u32 mip_levels =
          static_cast<u32>(
              std::floor(narrow<f64>(std::log2(std::max(cpu_image.width, cpu_image.height))))) +
          1;

      images[i] = renderer.upload_image(cpu_image, {.mip_levels = mip_levels});

      VkImageView image_view =
          vkh::create_image_view(
              renderer.context(),
              {.image = images[i],
               .format = VK_FORMAT_R8G8B8A8_SRGB,
               .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                          .level_count = mip_levels}})
              .value();
      image_views[i] = image_view;
    }
  }

  // A map from local index to resource index
  std::vector<uint32_t> texture_indices_map(cpu_scene.textures.size());
  {
    ZoneScopedN("Add textures");
    std::ranges::transform(
        cpu_scene.textures, texture_indices_map.begin(), [&](const CPUTexture& cpu_texture) {
          VkImage image = images.at(cpu_texture.image_index);
          VkImageView image_view = image_views.at(cpu_texture.image_index);
          return renderer.add_texture(Texture{.image = image, .image_view = image_view});
        });
  }
  const auto lookup_texture_index = [&](u32 local_index) {
    return texture_indices_map.at(local_index);
  };

  {
    ZoneScopedN("Upload materials");

    for (CPUMaterial material : cpu_scene.materials) {
      // TODO: offset texture index in a function

      material.albedo_texture_index = material.albedo_texture_index.map(lookup_texture_index);
      material.normal_texture_index = material.normal_texture_index.map(lookup_texture_index);
      material.metallic_roughness_texture_index =
          material.metallic_roughness_texture_index.map(lookup_texture_index);
      material.occlusion_texture_index = material.occlusion_texture_index.map(lookup_texture_index);
      [[maybe_unused]] u32 material_index = renderer.add_material(material);
    }

    renderer.upload_materials();
  }

  std::unordered_map<uint32_t, RenderComponent> render_components;

  for (u32 i = 0; i < cpu_scene.nodes.local_transforms.size(); ++i) {
    if (const i32 mesh_index = cpu_scene.nodes.mesh_indices[i]; mesh_index >= 0) {
      const auto mesh_handle = mesh_storage[mesh_index];
      render_components.insert({i, RenderComponent{.mesh = mesh_handle}});
    }
  }

  return Scene{
      .local_transforms = std::move(cpu_scene.nodes.local_transforms),
      .global_transforms = std::move(cpu_scene.nodes.global_transforms),
      .render_components = std::move(render_components),
  };
}

} // namespace charlie