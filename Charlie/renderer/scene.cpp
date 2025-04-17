#include "scene.hpp"

#include "../asset_handling/gltf_loader.hpp"
#include "../asset_handling/obj_loader.hpp"

#include "../utils/asset_path.hpp"
#include "../utils/background_tasks.hpp"

#include "../vulkan_helpers/initializers.hpp"

#include "beyond/math/serial.hpp"

#include "renderer.hpp"

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#include <fmt/chrono.h>
#include <latch>

namespace charlie {

[[nodiscard]] auto load_cpu_scene(std::string_view filename) -> CPUScene
{
  ZoneScoped;

  std::filesystem::path file_path = filename;
  if (file_path.is_relative()) {
    const auto& assets_path = get_asset_path();
    file_path = assets_path / file_path;
  }

  if (file_path.extension() == ".obj") {
    return load_obj(file_path);
  } else if (file_path.extension() == ".gltf" || file_path.extension() == ".glb") {
    return load_gltf(file_path);
  } else {
    beyond::panic("Unknown scene format!");
  }
}

[[nodiscard]] auto upload_scene(CPUScene&& cpu_scene, Renderer& renderer) -> Scene
{
  ZoneScoped;

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

  std::vector<uint32_t> material_index_map(cpu_scene.materials.size());
  {
    ZoneScopedN("Upload materials");

    for (u32 i = 0; i < cpu_scene.materials.size(); ++i) {
       auto& material = cpu_scene.materials[i];
      charlie::offset_material_texture_index(ref(material), lookup_texture_index);
      material_index_map[narrow<usize>(i)] = renderer.add_material(material);
    }

    renderer.upload_materials();
  }
  // Offset material indices
  offset_material_indices(ref(cpu_scene),
                          [&](u32 i) { return narrow<u32>(material_index_map[i]); });

  std::vector<MeshHandle> mesh_storage;
  {
    ZoneScopedN("Adds mesh");
    mesh_storage.reserve(cpu_scene.meshes.size());
    std::ranges::transform(cpu_scene.meshes, std::back_inserter(mesh_storage),
                           [&](const CPUMesh& mesh) { return renderer.add_mesh(mesh); });
  }

  std::unordered_map<uint32_t, RenderComponent> render_components;

  for (u32 i = 0; i < cpu_scene.nodes.local_transforms.size(); ++i) {
    if (const i32 mesh_index = cpu_scene.nodes.mesh_indices[i]; mesh_index >= 0) {
      const auto mesh_handle = mesh_storage[static_cast<usize>(mesh_index)];
      render_components.insert({i, RenderComponent{.mesh = mesh_handle}});
    }
  }

  auto mesh_buffers = renderer.upload_mesh_buffer(cpu_scene.buffers, "Scene");
  renderer.current_frame_deletion_queue().push(
      [buffers = renderer.scene_mesh_buffers](vkh::Context& context) {
        vkDestroyBuffer(context, buffers.position_buffer, nullptr);
        vkDestroyBuffer(context, buffers.vertex_buffer, nullptr);
        vkDestroyBuffer(context, buffers.index_buffer, nullptr);
      });
  renderer.scene_mesh_buffers = mesh_buffers;

  return Scene{
      .metadata = cpu_scene.metadata,
      .local_transforms = std::move(cpu_scene.nodes.local_transforms),
      .global_transforms = std::move(cpu_scene.nodes.global_transforms),
      .names = std::move(cpu_scene.nodes.names),
      .render_components = std::move(render_components),
  };
}

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer)
    -> beyond::expected<std::unique_ptr<Scene>, std::string>
{
  ZoneScoped;

  const auto start = std::chrono::steady_clock::now();

  CPUScene cpu_scene;
  try {
    cpu_scene = load_cpu_scene(filename);
  } catch (const SceneLoadingError& error) {
    return beyond::unexpected(std::string{error.what()});
  }

  auto scene = std::make_unique<Scene>(upload_scene(std::move(cpu_scene), renderer));
  const auto finish = std::chrono::steady_clock::now();

  SPDLOG_INFO("Load {} in {}", filename, std::chrono::duration<double, std::milli>{finish - start});

  return scene;
}

} // namespace charlie
