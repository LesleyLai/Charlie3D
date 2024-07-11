#ifndef CHARLIE3D_MESH_HPP
#define CHARLIE3D_MESH_HPP

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/buffer.hpp"

#include <beyond/math/vector.hpp>

namespace vkh {

class Context;

}

namespace charlie {

struct SubMesh {
  u32 vertex_offset = 0;
  u32 index_offset = 0;
  u32 index_count = 0;

  u32 material_index = 0;
};

struct Mesh {
  vkh::AllocatedBuffer position_buffer{};
  vkh::AllocatedBuffer vertex_buffer{};
  vkh::AllocatedBuffer index_buffer{};
  std::vector<SubMesh> submeshes;
};

void destroy_mesh(vkh::Context& context, Mesh& submesh);

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
