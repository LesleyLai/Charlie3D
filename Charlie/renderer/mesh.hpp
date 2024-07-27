#ifndef CHARLIE3D_MESH_HPP
#define CHARLIE3D_MESH_HPP

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/buffer.hpp"

#include <beyond/geometry/aabb3.hpp>
#include <beyond/math/vector.hpp>

namespace vkh {

class Context;

}

namespace charlie {

struct SubMesh {
  i32 vertex_offset = 0;
  u32 index_offset = 0;
  u32 index_count = 0;
  u32 material_index = 0;
};

struct Mesh {
  std::vector<SubMesh> submeshes;
  beyond::AABB3 aabb;
};

void destroy_mesh(vkh::Context& context, Mesh& submesh);

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
