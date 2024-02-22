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

struct MeshHandle : beyond::GenerationalHandle<MeshHandle, u32, 16> {
  using GenerationalHandle::GenerationalHandle;
};

struct RenderComponent {
  u32 transform_index = static_cast<u32>(-1);
  MeshHandle mesh;
};

struct Scene {
  std::vector<Mat4> local_transforms; // Cached local transformation for each node
  std::vector<Mat4> global_transforms;

  std::vector<RenderComponent> opaque_objects;
  std::unordered_map<u32, RenderComponent> render_components;
};

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene;

} // namespace charlie
#endif // CHARLIE3D_SCENE_HPP
