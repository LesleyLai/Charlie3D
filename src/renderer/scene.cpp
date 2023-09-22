#include "scene.hpp"

#include "../asset/gltf_loader.hpp"
#include "../asset/obj_loader.hpp"

#include "../utils/configuration.hpp"

#include "renderer.hpp"

namespace charlie {

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene
{
  std::filesystem::path file_path = filename;
  if (file_path.is_relative()) {
    const auto& assets_path =
        Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
    file_path = assets_path / file_path;
  }

  const CPUScene cpu_scene = [&]() {
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
  for (const auto& mesh : cpu_scene.meshes) {
    mesh_storage.push_back(renderer.upload_mesh_data(mesh));
  }

  std::unordered_map<uint32_t, RenderComponent> render_components;
  for (uint32_t i = 0; i < cpu_scene.objects.size(); ++i) {
    const auto& object = cpu_scene.objects[i];
    if (object.mesh_index >= 0) {
      render_components.insert(
          {i, RenderComponent{
                  .mesh = mesh_storage[beyond::narrow<uint32_t>(object.mesh_index)],
              }});
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