#ifndef CHARLIE3D_CPU_SCENE_HPP
#define CHARLIE3D_CPU_SCENE_HPP

#include <beyond/math/matrix.hpp>
#include <beyond/types/optional.hpp>
#include <vector>

#include "../utils/prelude.hpp"
#include "cpu_image.hpp"
#include "cpu_mesh.hpp"

namespace charlie {

enum class AlphaMode { opaque, mask, blend };

struct CPUMaterial {
  Vec4 base_color_factor;
  float metallic_factor = 0.0;
  float roughness_factor = 0.0;
  beyond::optional<u32> albedo_texture_index;
  beyond::optional<u32> normal_texture_index;
  beyond::optional<u32> metallic_roughness_texture_index;
  beyond::optional<u32> occlusion_texture_index;

  AlphaMode alpha_mode = AlphaMode::opaque;
  float alpha_cutoff = 0.0f; // Only considered when alpha mode is mask
};

struct CPUTexture {
  std::string name;
  u32 image_index = 0;
  beyond::optional<u32> sampler_index = beyond::nullopt;
};

// SOA for a node structure
struct Nodes {
  std::vector<std::string> names;
  std::vector<Mat4> local_transforms;
  std::vector<Mat4> global_transforms;
  std::vector<i32> mesh_indices; // -1 for no mesh
};

// Used to add new nodes to scene
struct NodeInfo {
  std::string name;
  Mat4 local_transform;
  i32 parent_index = -1; // -1 for no parent
  i32 mesh_index = -1;   // -1 for no mesh
};

// Mirrors the scene-graph structure but all data here are on CPU
struct CPUScene {
  Nodes nodes;
  std::vector<u32> root_node_indices; // index of root nodes

  std::vector<CPUMesh> meshes;
  std::vector<CPUMaterial> materials;
  std::vector<CPUImage> images;
  std::vector<CPUTexture> textures;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
