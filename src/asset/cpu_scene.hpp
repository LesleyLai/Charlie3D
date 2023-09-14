#ifndef CHARLIE3D_CPU_SCENE_HPP
#define CHARLIE3D_CPU_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <vector>

#include "cpu_mesh.hpp"

namespace charlie {

struct CPUObject {
  int mesh_index = -1; // Index to mesh
};

// Mirrors the scene-graph structure but all data here are on CPU
struct CPUScene {
  std::vector<beyond::Mat4> local_transforms;
  std::vector<CPUObject> objects;

  std::vector<CPUMesh> meshes;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
