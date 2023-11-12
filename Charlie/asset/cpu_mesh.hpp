#ifndef CHARLIE3D_CPU_MESH_HPP
#define CHARLIE3D_CPU_MESH_HPP

#include <beyond/math/vector.hpp>

#include <string>
#include <vector>

#include <beyond/geometry/aabb3.hpp>
#include <beyond/types/optional.hpp>

#include <beyond/container/static_vector.hpp>

#include "../utils/prelude.hpp"

namespace charlie {

struct CPUMesh {
  std::string name;

  beyond::optional<u32> material_index;

  std::vector<Point3> positions;
  std::vector<Vec3> normals;
  std::vector<Vec2> uv;
  std::vector<Vec4> tangents;

  std::vector<u32> indices;
  beyond::AABB3 bounding_box; // object space bounding box
};

} // namespace charlie

#endif // CHARLIE3D_CPU_MESH_HPP
