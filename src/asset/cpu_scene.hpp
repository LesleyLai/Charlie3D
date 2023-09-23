#ifndef CHARLIE3D_CPU_SCENE_HPP
#define CHARLIE3D_CPU_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/types/optional.hpp>
#include <vector>

#include "cpu_mesh.hpp"

namespace charlie {

struct CPUObject {
  int mesh_index = -1; // Index to mesh
};

struct CPUMaterial {
  beyond::Vec4 base_color_factor;
  beyond::optional<uint32_t> albedo_texture_index;
};

struct CPUImage {
  int width = 0;
  int height = 0;
  int compoments = 0;
  std::unique_ptr<const uint8_t[]> data;
};

// Mirrors the scene-graph structure but all data here are on CPU
struct CPUScene {
  std::vector<beyond::Mat4> local_transforms;
  std::vector<CPUObject> objects;

  std::vector<CPUMesh> meshes;
  std::vector<CPUMaterial> materials;
  std::vector<CPUImage> images;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
