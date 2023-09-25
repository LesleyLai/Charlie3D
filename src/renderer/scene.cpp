#include "scene.hpp"

#include "../asset/gltf_loader.hpp"
#include "../asset/obj_loader.hpp"

#include "../utils/configuration.hpp"

#include "vulkan_helpers/image_view.hpp"

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

  const uint32_t texture_index_offset = renderer.texture_count();
  for (const auto& cpu_textures : cpu_scene.textures) {
    const VkImage image = images.at(cpu_textures.image_index);
    const VkImageView image_view =
        vkh::create_image_view(
            renderer.context(),
            {.image = image,
             .format = VK_FORMAT_R8G8B8A8_SRGB,
             .subresource_range = vkh::SubresourceRange{.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT}})
            .value();

    const Texture texture{.image = image, .image_view = image_view};
    renderer.add_texture(texture);
  }

  std::unordered_map<uint32_t, RenderComponent> render_components;

  for (uint32_t i = 0; i < cpu_scene.objects.size(); ++i) {
    const auto& object = cpu_scene.objects[i];
    if (object.mesh_index >= 0) {
      const auto mesh_handle = mesh_storage[beyond::narrow<uint32_t>(object.mesh_index)];

      const auto material_index = cpu_scene.meshes.at(object.mesh_index).material_index.value();
      // TODO: handle model without albedo textures
      const auto albedo_texture_index =
          cpu_scene.materials[material_index]
              .albedo_texture_index
              .map([&](uint32_t index) { return index + texture_index_offset; })
              .value_or(0);

      render_components.insert(
          {i, RenderComponent{.mesh = mesh_handle, .albedo_texture_index = albedo_texture_index}});
    }
  }

  auto local_transforms = std::move(cpu_scene.local_transforms);
  std::vector<beyond::Mat4> global_transforms(local_transforms);
  // TODO: generate global transform from local transform

  return Scene{
      .local_transforms = std::move(local_transforms),
      .global_transforms = std::move(global_transforms),
      .render_components_ = std::move(render_components),
  };
}

} // namespace charlie