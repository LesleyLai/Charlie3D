#ifndef CHARLIE3D_MESH_HPP
#define CHARLIE3D_MESH_HPP

#include <beyond/math/vector.hpp>

#include <array>
#include <span>

#include "vulkan_helpers/buffer.hpp"

namespace vkh {

class Context;

}

namespace charlie {

struct Mesh {
  vkh::AllocatedBuffer position_buffer{};
  vkh::AllocatedBuffer normal_buffer{};
  vkh::AllocatedBuffer uv_buffer{};

  vkh::AllocatedBuffer index_buffer{};
  std::uint32_t vertices_count{};
  std::uint32_t index_count{};
};

void destroy_mesh(vkh::Context& context, const Mesh& mesh);

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
