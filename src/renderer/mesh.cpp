#include "../utils/configuration.hpp"

#include <filesystem>

#include "mesh.hpp"
#include "tiny_obj_loader.h"

namespace charlie {

auto CPUMesh::load(std::string_view filename) -> CPUMesh
{
  const auto& assets_path =
      Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
  const auto file_path = assets_path / filename;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn;
  std::string err;

  auto file_directory = file_path;
  file_directory.remove_filename();

  if (const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn,
                                        &err, file_path.string().c_str(),
                                        file_directory.string().c_str());
      !ret) {
    beyond::panic(fmt::format("Mesh loading error: {}", err));
  }

  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  for (const auto& shape : shapes) {
    std::size_t index_offset = 0;
    for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      const auto fv = std::size_t(shape.mesh.num_face_vertices[f]);
      for (std::size_t v = 0; v < fv; v++) {
        const tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        const auto vx = attrib.vertices[3 * idx.vertex_index + 0];
        const auto vy = attrib.vertices[3 * idx.vertex_index + 1];
        const auto vz = attrib.vertices[3 * idx.vertex_index + 2];

        const auto nx = attrib.normals[3 * idx.normal_index + 0];
        const auto ny = attrib.normals[3 * idx.normal_index + 1];
        const auto nz = attrib.normals[3 * idx.normal_index + 2];

        // vertex uv
        const auto ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        const auto uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        vertices.push_back(Vertex{.position = {vx, vy, vz},
                                  .normal = {nx, ny, nz},
                                  .uv = {ux, 1 - uy}});
      }
      index_offset += fv;
    }
  }

  return CPUMesh{.vertices = std::move(vertices),
                 .indices = std::move(indices)};
}

} // namespace charlie
