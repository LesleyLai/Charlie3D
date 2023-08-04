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

struct Vertex {
  beyond::Vec3 position;
  beyond::Vec3 normal;
  beyond::Vec2 uv;

  [[nodiscard]] static constexpr auto binding_description()
  {
    return VkVertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
  }

  [[nodiscard]] static constexpr auto attributes_descriptions()
  {
    return std::to_array<VkVertexInputAttributeDescription>(
        {{.location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = beyond::to_u32(offsetof(Vertex, position))},
         {.location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = beyond::to_u32(offsetof(Vertex, normal))},
         {.location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = beyond::to_u32(offsetof(Vertex, uv))}});
  }
};

struct CPUMesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  [[nodiscard]] static auto load(const char* filename) -> CPUMesh;
};

struct Mesh {
  vkh::Buffer vertex_buffer{};
  vkh::Buffer index_buffer{};
  std::uint32_t vertices_count{};
  std::uint32_t index_count{};
};

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
