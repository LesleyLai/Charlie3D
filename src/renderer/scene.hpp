#ifndef CHARLIE3D_SCENE_HPP
#define CHARLIE3D_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/utils/handle.hpp>
#include <beyond/utils/narrowing.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "mesh.hpp"

#include <vulkan/vulkan_core.h>

#include <beyond/container/slot_map.hpp>

namespace charlie {

class Renderer;

struct MeshHandle : beyond::GenerationalHandle<MeshHandle, uint32_t, 16> {
  using GenerationalHandle::GenerationalHandle;
};

struct ImageHandle : beyond::GenerationalHandle<MeshHandle, uint32_t, 16> {
  using GenerationalHandle::GenerationalHandle;
};

struct RenderComponent {
  MeshHandle mesh;
  uint32_t albedo_texture_index = 0;
};

struct Scene {
  std::vector<beyond::Mat4> local_transforms; // Cached local transformation for each nodes
  std::vector<beyond::Mat4> global_transforms;
  std::unordered_map<uint32_t, RenderComponent> render_components_;
};

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene;

} // namespace charlie
#endif // CHARLIE3D_SCENE_HPP
