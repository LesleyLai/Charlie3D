#include "obj_loader.hpp"

#include "beyond/utils/narrowing.hpp"

#include <meshoptimizer.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace charlie {

auto index_mesh(charlie::CPUMesh& mesh)
{

  meshopt_Stream streams[] = {
      {mesh.positions.data(), sizeof(float) * 3, sizeof(float) * 3},
      {mesh.normals.data(), sizeof(float) * 3, sizeof(float) * 3},
      {mesh.uv.data(), sizeof(float) * 2, sizeof(float) * 2},
  };

  const auto initial_vertex_count = beyond::narrow<unsigned int>(mesh.positions.size());
  const auto index_count = initial_vertex_count;

  std::vector<unsigned int> remap(index_count);
  const size_t vertex_count = meshopt_generateVertexRemapMulti(
      remap.data(), mesh.indices.empty() ? nullptr : mesh.indices.data(), index_count,
      initial_vertex_count, streams, sizeof(streams) / sizeof(streams[0]));

  meshopt_remapVertexBuffer(mesh.positions.data(), mesh.positions.data(), initial_vertex_count,
                            sizeof(float) * 3, remap.data());

  meshopt_remapVertexBuffer(mesh.normals.data(), mesh.normals.data(), initial_vertex_count,
                            sizeof(float) * 3, remap.data());

  meshopt_remapVertexBuffer(mesh.uv.data(), mesh.uv.data(), initial_vertex_count, sizeof(float) * 2,
                            remap.data());

  mesh.positions.resize(vertex_count);
  mesh.positions.shrink_to_fit();

  mesh.normals.resize(vertex_count);
  mesh.normals.shrink_to_fit();

  mesh.uv.resize(vertex_count);
  mesh.uv.shrink_to_fit();

  mesh.indices.resize(index_count);
  meshopt_remapIndexBuffer(mesh.indices.data(), nullptr, index_count, remap.data());
}

[[nodiscard]] auto load_obj(const std::filesystem::path& file_path) -> CPUScene
{
  auto file_directory = file_path;
  file_directory.remove_filename();

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn;
  std::string err;

  if (const bool ret =
          tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file_path.string().c_str(),
                           file_directory.string().c_str());
      !ret) {
    beyond::panic(fmt::format("Mesh loading error: {}", err));
  }

  std::vector<charlie::CPUMesh> meshes;
  for (const auto& shape : shapes) {

    const auto first_material_id = shape.mesh.material_ids.front();
    const auto res =
        std::ranges::find_if(shape.mesh.material_ids,
                             [first_material_id](const int id) { return id != first_material_id; });
    if (res != shape.mesh.material_ids.end()) {
      beyond::panic("We can't handle multiple material ids in one mesh yet!\n");
    }
    // const auto& material = materials[first_material_id];

    std::vector<beyond::Point3> positions;
    std::vector<beyond::Vec3> normals;
    std::vector<beyond::Vec2> uv;

    usize index_offset = 0;
    for (usize f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      const auto fv = usize(shape.mesh.num_face_vertices[f]);
      BEYOND_ENSURE(fv == 3);
      for (usize v = 0; v < fv; v++) {
        const tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        const auto vx = attrib.vertices[3 * idx.vertex_index + 0];
        const auto vy = attrib.vertices[3 * idx.vertex_index + 1];
        const auto vz = attrib.vertices[3 * idx.vertex_index + 2];

        const auto nx = attrib.normals[3 * idx.normal_index + 0];
        const auto ny = attrib.normals[3 * idx.normal_index + 1];
        const auto nz = attrib.normals[3 * idx.normal_index + 2];

        const auto ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        const auto uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        positions.emplace_back(vx, vy, vz);
        normals.emplace_back(nx, ny, nz);
        uv.emplace_back(ux, 1 - uy);
      }
      index_offset += fv;
    }

    auto cpu_mesh = charlie::CPUMesh{
        .name = shape.name,
        .positions = std::move(positions),
        .normals = std::move(normals),
        .uv = std::move(uv),
    };
    index_mesh(cpu_mesh);
    meshes.push_back(std::move(cpu_mesh));
  }

  std::vector<CPURenderObject> objects;
  objects.reserve(meshes.size() + 1);
  objects.push_back(CPURenderObject{.mesh_index = -1}); // root
  for (size_t i = 0; i < meshes.size(); ++i) {
    objects.push_back(CPURenderObject{
        .mesh_index = beyond::narrow<int>(i),
    });
  }

  std::vector<beyond::Mat4> local_transforms(objects.size(), beyond::Mat4::identity());

  return CPUScene{
      .local_transforms = std::move(local_transforms),
      .objects = std::move(objects),
      .meshes = std::move(meshes),
  };
}

} // namespace charlie