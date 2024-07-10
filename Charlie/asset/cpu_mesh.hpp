#ifndef CHARLIE3D_CPU_MESH_HPP
#define CHARLIE3D_CPU_MESH_HPP

#include <beyond/math/vector.hpp>

#include <string>
#include <vector>

#include <beyond/geometry/aabb3.hpp>
#include <beyond/types/optional.hpp>

#include <beyond/container/static_vector.hpp>

#include "../utils/prelude.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace charlie {

// Interleaved vertex attributes except position (which is in a stand-alone stream)
struct Vertex {
  alignas(Vec4) Vec3 normal;
  alignas(Vec4) Vec2 tex_coords;
  alignas(Vec4) Vec4 tangents;
};

struct CPUSubmesh {
  beyond::optional<u32> material_index;

  std::vector<Point3> positions;
  std::vector<Vertex> vertices;

  std::vector<u32> indices;
};

struct CPUMesh {
  std::string name;
  std::vector<CPUSubmesh> submeshes;
};

} // namespace charlie

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // CHARLIE3D_CPU_MESH_HPP
