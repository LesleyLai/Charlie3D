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
};

struct CPUTexture {
  std::string name;
  u32 image_index = 0;
  beyond::optional<u32> sampler_index = beyond::nullopt;
};

// Information about a node's location in the tree
struct NodeCoordinate {
  i32 parent_ = -1; // -1 for no parent
  i32 first_child_ = -1;
  i32 next_sibling_ = -1;
  i32 level_ = -1; // cached level
};

// SOA for a node structure
struct Nodes {
  std::vector<NodeCoordinate> hierarchy;
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

  // Returns the index of the node
  auto add_node(const NodeInfo& node_info) -> u32
  {
    nodes.hierarchy.push_back({
        .parent_ = node_info.parent_index,
    });
    const auto node_index = narrow<u32>(nodes.hierarchy.size() - 1);

    nodes.names.push_back(node_info.name);
    nodes.local_transforms.push_back(node_info.local_transform);

    auto global_transform = node_info.local_transform;
    if (node_info.parent_index >= 0) { // has parent
      global_transform = node_info.local_transform *
                         nodes.global_transforms.at(narrow<usize>(node_info.parent_index));
    } else {
      root_node_indices.push_back(node_index);
    }
    nodes.global_transforms.push_back(global_transform);
    nodes.mesh_indices.push_back(node_info.mesh_index);

    return node_index;
  }
};

} // namespace charlie

#endif // CHARLIE3D_CPU_SCENE_HPP
