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

#include "../asset_handling/cpu_scene.hpp"

namespace charlie {

class Renderer;

struct MeshHandle : beyond::GenerationalHandle<MeshHandle, u32, 16> {
  using GenerationalHandle::GenerationalHandle;
};

struct RenderComponent {
  MeshHandle mesh;
};

// Runtime scene representation
// This is an ECS-like structure where each scene node is represented as an index
struct Scene {
  std::vector<Mat4> local_transforms; // Cached local transformation for each node
  std::vector<Mat4> global_transforms;
  // std::vector<NodeCoordinate> hierarchy;
  std::vector<std::string> names;

  // Combined vertex/index buffers
  vkh::AllocatedBuffer position_buffer{};
  vkh::AllocatedBuffer vertex_buffer{};
  vkh::AllocatedBuffer index_buffer{};

  std::unordered_map<u32, RenderComponent> render_components;

  [[nodiscard]] auto node_count() const -> u32
  {
    const auto size = local_transforms.size();
    BEYOND_ENSURE(global_transforms.size() == size);
    return narrow<u32>(size);
  }
};

[[nodiscard]] auto load_scene(std::string_view filename, Renderer& renderer) -> Scene;

} // namespace charlie
#endif // CHARLIE3D_SCENE_HPP
