#ifndef CHARLIE3D_CPU_SCENE_HPP
#define CHARLIE3D_CPU_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/types/optional.hpp>
#include <vector>

#include "../utils/prelude.hpp"
#include "cpu_image.hpp"
#include "cpu_mesh.hpp"

namespace charlie {

struct CPURenderObject {
  int mesh_index = -1; // Index to mesh
};

struct CPUMaterial {
  Vec4 base_color_factor;
  beyond::optional<u32> albedo_texture_index;
  beyond::optional<u32> normal_texture_index;
  beyond::optional<u32> occlusion_texture_index;
};

struct CPUTexture {
  std::string name;
  u32 image_index = 0;
  u32 sampler_index = 0;
};

// Mirrors the scene-graph structure but all data here are on CPU
struct CPUScene {
  std::vector<Mat4> local_transforms;
  std::vector<CPURenderObject> objects;

  std::vector<CPUMesh> meshes;
  std::vector<CPUMaterial> materials;
  std::vector<CPUImage> images;
  std::vector<CPUTexture> textures;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
