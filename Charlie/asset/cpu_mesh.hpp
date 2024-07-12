#ifndef CHARLIE3D_CPU_MESH_HPP
#define CHARLIE3D_CPU_MESH_HPP

#include <beyond/math/vector.hpp>

#include <string>
#include <vector>

#include <beyond/geometry/aabb3.hpp>
#include <beyond/math/vector.hpp>
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
  Vec2 normal;
  Vec2 tex_coords;
  alignas(Vec4) Vec4 tangents;
};

// TODO: test this
[[nodiscard]] constexpr auto sign_not_zero(Vec2 v)
{
  return Vec2((v.x >= 0.0f) ? +1.0f : -1.0f, (v.y >= 0.0f) ? +1.0f : -1.0f);
}

// TODO: test this
[[nodiscard]] inline auto vec3_to_oct(Vec3 v) -> Vec2
{
  const Vec2 p = Vec2{v.x, v.y} * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
  if (v.z <= 0.0f) {
    const auto sign = sign_not_zero(p);
    const float x = (1.0f - std::abs(p.y)) * sign.x;
    const float y = (1.0f - std::abs(p.x)) * sign.y;
    return Vec2{x, y};
  } else {
    return p;
  }
}

struct CPUSubmesh {
  beyond::optional<u32> material_index;

  u32 vertex_offset = 0;
  u32 index_offset = 0;
  u32 index_count = 0;
};

struct CPUMesh {
  std::string name;
  std::vector<CPUSubmesh> submeshes;
};

// Buffers for a single combined mesh
// Each gltf/glb/obj file has concatenated buffers
struct CPUMeshBuffers {
  std::vector<Point3> positions; // Seperate position from Rest of the vertex attributes
  std::vector<Vertex> vertices;
  std::vector<u32> indices;
};

} // namespace charlie

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // CHARLIE3D_CPU_MESH_HPP
