#ifndef CHARLIE3D_MESH_HPP
#define CHARLIE3D_MESH_HPP

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/buffer.hpp"

#include <beyond/math/vector.hpp>

#include <array>
#include <span>

namespace vkh {

class Context;

}

namespace charlie {

struct SubMesh {
  vkh::AllocatedBuffer position_buffer{};
  vkh::AllocatedBuffer normal_buffer{};
  vkh::AllocatedBuffer uv_buffer{};
  vkh::AllocatedBuffer tangent_buffer{};

  vkh::AllocatedBuffer index_buffer{};
  u32 vertices_count{};
  u32 index_count{};

  u32 material_index = 0;
};

struct Mesh {
  std::vector<SubMesh> submeshes;
};

void destroy_submesh(vkh::Context& context, SubMesh& submesh);

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
