#ifndef CHARLIE3D_CPU_SCENE_HPP
#define CHARLIE3D_CPU_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/types/optional.hpp>
#include <vector>

#include "cpu_mesh.hpp"

namespace charlie {

struct CPURenderObject {
  int mesh_index = -1; // Index to mesh
};

struct CPUMaterial {
  beyond::Vec4 base_color_factor;
  beyond::optional<uint32_t> albedo_texture_index;
};

struct CPUImage {
  std::string name;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t compoments = 0;
  std::unique_ptr<const uint8_t[]> data;
};

struct CPUTexture {
  std::string name;
  uint32_t image_index = 0;
  uint32_t sampler_index = 0;
};

// Mirrors the scene-graph structure but all data here are on CPU
struct CPUScene {
  std::vector<beyond::Mat4> local_transforms;
  std::vector<CPURenderObject> objects;

  std::vector<CPUMesh> meshes;
  std::vector<CPUMaterial> materials;
  std::vector<CPUImage> images;
  std::vector<CPUTexture> textures;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
