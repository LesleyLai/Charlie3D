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

struct Vertex {
  beyond::Vec3 position;
  beyond::Vec3 normal;
  beyond::Vec3 color;
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
          .offset = offsetof(Vertex, position)},
         {.location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, normal)},
         {.location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, color)},
         {.location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, uv)}});
  }
};

struct Mesh {
  vkh::Buffer vertex_buffer{};
  vkh::Buffer index_buffer{};
  std::uint32_t vertices_count{};
  std::uint32_t index_count{};
};

} // namespace charlie

#endif // CHARLIE3D_MESH_HPP
