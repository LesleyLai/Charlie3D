#ifndef CHARLIE3D_CPU_MESH_HPP
#define CHARLIE3D_CPU_MESH_HPP

#include <beyond/math/vector.hpp>

#include <string>
#include <vector>

#include <beyond/types/optional.hpp>

namespace charlie {

struct CPUMesh {
  std::string name;

  beyond::optional<uint32_t> material_index;

  std::vector<beyond::Vec3> positions;
  std::vector<beyond::Vec3> normals;
  std::vector<beyond::Vec2> uv;
  std::vector<beyond::Vec4> tangents;

  std::vector<std::uint32_t> indices;
};

} // namespace charlie

#endif // CHARLIE3D_CPU_MESH_HPP
