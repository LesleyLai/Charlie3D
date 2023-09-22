#include "gltf_loader.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <beyond/math/matrix.hpp>
#include <beyond/utils/narrowing.hpp>

namespace fastgltf {

template <>
struct ElementTraits<beyond::Vec3> : ElementTraitsBase<beyond::Vec3, AccessorType::Vec3, float> {};

} // namespace fastgltf

namespace charlie {

[[nodiscard]] auto get_node_transform(const fastgltf::Node& node) -> beyond::Mat4
{
  if (const auto* transform_ptr = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform);
      transform_ptr != nullptr) {
    const auto transform_arr = *transform_ptr;

    return beyond::Mat4{transform_arr[0],  transform_arr[1],  transform_arr[2],  transform_arr[3],
                        transform_arr[4],  transform_arr[5],  transform_arr[6],  transform_arr[7],
                        transform_arr[8],  transform_arr[9],  transform_arr[10], transform_arr[11],
                        transform_arr[12], transform_arr[13], transform_arr[14], transform_arr[15]};

  } else {
    //    const auto transform = std::get<fastgltf::Node::TRS>(node.transform);
    //    const beyond::Vec3 translation{transform.translation[0], transform.translation[1],
    //                                   transform.translation[2]};
    //    const beyond::Quat rotation{transform.rotation[3], transform.rotation[0],
    //    transform.rotation[1],
    //                                transform.rotation[2]};
    //    const beyond::Vec3 scale{transform.scale[0], transform.scale[1], transform.scale[2]};

    // TODO: proper handle TRS
    return beyond::Mat4::identity();
  }
}

[[nodiscard]] auto load_gltf(const std::filesystem::path& file_path) -> CPUScene
{
  ZoneScoped;

  // Creates a Parser instance. Optimally, you should reuse this across loads, but don't use it
  // across threads. To enable extensions, you have to pass them into the parser's constructor.
  fastgltf::Parser parser;

  // The GltfDataBuffer class is designed for re-usability of the same JSON string. It contains
  // utility functions to load data from a std::filesystem::path, copy from an existing buffer,
  // or re-use an already existing allocation. Note that it has to outlive the process of every
  // parsing function you call.
  fastgltf::GltfDataBuffer data;
  data.loadFromFile(file_path);

  // This loads the glTF file into the gltf object and parses the JSON. For GLB files, use
  // Parser::loadBinaryGLTF instead.
  using fastgltf::Options;
  auto asset = parser.loadGLTF(&data, file_path.parent_path(),
                               Options::LoadGLBBuffers | Options::LoadExternalBuffers |
                                   Options::LoadExternalImages);

  if (auto error = asset->parse(); error != fastgltf::Error::None) {
    SPDLOG_ERROR("Error while loading {}: {}", file_path.string(), static_cast<uint64_t>(error));
  }

  const std::unique_ptr parsed_asset = asset->getParsedAsset();

  CPUScene result;

  for (auto& node : parsed_asset->nodes) {
    const auto transform = get_node_transform(node);

    result.objects.push_back({.mesh_index = node.meshIndex.has_value()
                                                ? beyond::narrow<int>(node.meshIndex.value())
                                                : -1});

    result.local_transforms.push_back(transform);
  }

  //  for (const auto& material : parsed_asset->materials) {
  //    fmt::print("{}\n", material.name);
  //    BEYOND_ENSURE(material.pbrData.has_value());
  //    fmt::print("{}\n", fmt::join(material.pbrData->baseColorFactor, ", "));
  //  }

  for (const auto& mesh : parsed_asset->meshes) {
    // TODO: handle more primitives
    BEYOND_ENSURE(mesh.primitives.size() == 1);

    const auto& primitive = mesh.primitives.at(0);
    BEYOND_ENSURE(primitive.type == fastgltf::PrimitiveType::Triangles);

    size_t position_accessor_id = 0;
    size_t normal_accessor_id = 0;
    size_t index_accessor_id = 0;
    if (const auto itr = primitive.attributes.find("POSITION"); itr != primitive.attributes.end()) {
      position_accessor_id = itr->second;
    } else {
      beyond::panic("Mesh misses POSITION attribute!");
    }
    if (const auto itr = primitive.attributes.find("NORMAL"); itr != primitive.attributes.end()) {
      normal_accessor_id = itr->second;
    } else {
      beyond::panic("Mesh misses NORMAL attribute!");
    }
    // TODO: handle meshes without index accessor
    index_accessor_id = primitive.indicesAccessor.value();

    const fastgltf::Accessor& position_accessor = parsed_asset->accessors.at(position_accessor_id);
    const fastgltf::Accessor& normal_accessor = parsed_asset->accessors.at(normal_accessor_id);
    const fastgltf::Accessor& index_accessor = parsed_asset->accessors.at(index_accessor_id);

    BEYOND_ENSURE(position_accessor.type == fastgltf::AccessorType::Vec3);
    BEYOND_ENSURE(normal_accessor.type == fastgltf::AccessorType::Vec3);
    BEYOND_ENSURE(index_accessor.type == fastgltf::AccessorType::Scalar);

    std::vector<beyond::Vec3> positions;
    positions.resize(position_accessor.count);
    fastgltf::copyFromAccessor<beyond::Vec3>(*parsed_asset, position_accessor, positions.data());

    std::vector<beyond::Vec3> normals;
    normals.resize(normal_accessor.count);
    fastgltf::copyFromAccessor<beyond::Vec3>(*parsed_asset, normal_accessor, normals.data());

    // TODO: proper texture coordinates
    std::vector<beyond::Vec2> tex_coords(positions.size());

    std::vector<std::uint32_t> indices;
    indices.resize(index_accessor.count);
    fastgltf::copyFromAccessor<std::uint32_t>(*parsed_asset, index_accessor, indices.data());

    result.meshes.push_back(CPUMesh{.name = mesh.name,
                                    .positions = std::move(positions),
                                    .normals = std::move(normals),
                                    .uv = std::move(tex_coords),
                                    .indices = std::move(indices)});
  }

  if (auto error = asset->validate(); error != fastgltf::Error::None) {
    SPDLOG_ERROR("GLTF validation error {} from{}", static_cast<uint64_t>(error),
                 file_path.string().c_str());
  }

  SPDLOG_INFO("GLTF loaded from {}", file_path.string().c_str());
  return result;
}

} // namespace charlie