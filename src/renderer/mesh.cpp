#include "mesh.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "vulkan_helpers/context.hpp"

[[nodiscard]] auto load_mesh(vkh::Context& context, const char* filename)
    -> Mesh
{
  Assimp::Importer importer;

  const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate);
  if (!scene || !scene->HasMeshes()) {
    beyond::panic(fmt::format("Unable to load {}", filename));
  }
  const aiMesh* mesh = scene->mMeshes[0];

  std::vector<Vertex> vertices;
  for (unsigned i = 0; i != mesh->mNumVertices; i++) {
    const aiVector3D v = mesh->mVertices[i];
    // const aiVector3D t = mesh->mTextureCoords[0][i];
    vertices.push_back(
        Vertex{.position = {v.x, v.z, v.y}, .color = {1.f, 0.f, 0.f}});
  }

  std::vector<std::uint32_t> indices;
  for (unsigned i = 0; i != mesh->mNumFaces; i++)
    for (unsigned j = 0; j != 3; j++)
      indices.push_back(mesh->mFaces[i].mIndices[j]);

  return Mesh{.vertex_buffer = vkh::create_buffer_from_data(
                                   context,
                                   {.size = vertices.size() * sizeof(Vertex),
                                    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                    .debug_name = "Mesh Vertex Buffer"},
                                   vertices.data())
                                   .value(),
              .index_buffer =
                  vkh::create_buffer_from_data(
                      context,
                      {.size = indices.size() * sizeof(std::uint32_t),
                       .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                       .debug_name = "Mesh Index Buffer"},
                      indices.data())
                      .value(),
              .index_count = static_cast<std::uint32_t>(indices.size())

  };
}