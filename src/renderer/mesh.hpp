#ifndef CHARLIE3D_MESH_HPP
#define CHARLIE3D_MESH_HPP

#include <beyond/math/vector.hpp>
#include <beyond/utils/conversion.hpp>

#include <array>
#include <span>

#include "vulkan_helpers/buffer.hpp"

namespace vkh {

class Context;

}

namespace charlie {

struct CPUMesh {
  std::vector<beyond::Vec3> positions;
  std::vector<beyond::Vec3> normals;
  std::vector<beyond::Vec2> uv;

  std::vector<std::uint32_t> indices;

  [[nodiscard]] static auto load(std::string_view filename) -> CPUMesh;
};

struct Mesh {
  vkh::Buffer position_buffer{};
  vkh::Buffer normal_buffer{};
  vkh::Buffer uv_buffer{};

  vkh::Buffer index_buffer{};
  std::uint32_t vertices_count{};
  std::uint32_t index_count{};
};

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
