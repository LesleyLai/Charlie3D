#include "gltf_loader.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <beyond/math/matrix.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/utils/narrowing.hpp>

#include <stb_image.h>

namespace fastgltf {

template <>
struct ElementTraits<beyond::Vec2> : ElementTraitsBase<beyond::Vec2, AccessorType::Vec2, float> {};

template <>
struct ElementTraits<beyond::Vec3> : ElementTraitsBase<beyond::Vec3, AccessorType::Vec3, float> {};

} // namespace fastgltf

namespace {

auto to_cpu_texture(const fastgltf::Texture& texture) -> charlie::CPUTexture
{
  return {
      .name = texture.name,
      .image_index = beyond::narrow<uint32_t>(texture.imageIndex.value()),
      .sampler_index = beyond::narrow<uint32_t>(texture.samplerIndex.value()),
  };
};

auto load_raw_image_data(const std::filesystem::path& gltf_directory, const fastgltf::Image& image)
    -> charlie::CPUImage
{
  ZoneScoped;

  return std::visit(
      [&](const auto& data) -> charlie::CPUImage {
        using DataType = std::remove_cvref_t<decltype(data)>;
        if constexpr (std::is_same_v<DataType, fastgltf::sources::URI>) {
          std::filesystem::path file_path =
              data.uri.isLocalPath() ? gltf_directory / data.uri.fspath() : data.uri.fspath();
          file_path = std::filesystem::canonical(file_path);

          int width, height, components;
          uint8_t* pixels =
              stbi_load(file_path.string().c_str(), &width, &height, &components, STBI_rgb_alpha);
          BEYOND_ENSURE(pixels != nullptr);

          SPDLOG_INFO("Load Image from {}", file_path.string());

          return charlie::CPUImage{
              .name = image.name.empty() ? file_path.string() : image.name,
              .width = beyond::narrow<uint32_t>(width),
              .height = beyond::narrow<uint32_t>(height),
              .compoments = beyond::narrow<uint32_t>(components),
              .data = std::unique_ptr<uint8_t[]>(pixels),
          };

        } else if constexpr (std::is_same_v<DataType, fastgltf::sources::Vector>) {
          // TODO: Handle other Mime types
          BEYOND_ENSURE(data.mimeType == fastgltf::MimeType::GltfBuffer);

          int width, height, components;
          uint8_t* pixels = stbi_load_from_memory(data.bytes.data(), data.bytes.size(), &width,
                                                  &height, &components, STBI_rgb_alpha);
          BEYOND_ENSURE(pixels != nullptr);

          return charlie::CPUImage{
              .name = image.name,
              .width = beyond::narrow<uint32_t>(width),
              .height = beyond::narrow<uint32_t>(height),
              .compoments = beyond::narrow<uint32_t>(components),
              .data = std::unique_ptr<uint8_t[]>(pixels),
          };

        } else {
          beyond::panic("Unsupported image data format!");
        }
      },
      image.data);
}

} // anonymous namespace

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
                               Options::LoadGLBBuffers | Options::LoadExternalBuffers);

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

  result.images.reserve(parsed_asset->images.size());
  std::ranges::transform(parsed_asset->images, std::back_inserter(result.images),
                         std::bind_front(load_raw_image_data, file_path.parent_path()));

  result.textures.reserve(parsed_asset->textures.size());
  std::ranges::transform(parsed_asset->textures, std::back_inserter(result.textures),
                         to_cpu_texture);

  result.materials.reserve(parsed_asset->materials.size());
  for (const auto& material : parsed_asset->materials) {
    BEYOND_ENSURE(material.pbrData.has_value());

    beyond::optional<uint32_t> albedo_texture_index;
    if (material.pbrData->baseColorTexture.has_value()) {
      albedo_texture_index =
          beyond::narrow<uint32_t>(material.pbrData->baseColorTexture->textureIndex);
    }

    result.materials.push_back(
        CPUMaterial{.base_color_factor = {material.pbrData->baseColorFactor[0],
                                          material.pbrData->baseColorFactor[1],
                                          material.pbrData->baseColorFactor[2],
                                          material.pbrData->baseColorFactor[3]},
                    .albedo_texture_index = albedo_texture_index});
  }

  for (const auto& mesh : parsed_asset->meshes) {
    // TODO: handle more primitives
    BEYOND_ENSURE(mesh.primitives.size() == 1);

    const auto& primitive = mesh.primitives.at(0);
    BEYOND_ENSURE(primitive.type == fastgltf::PrimitiveType::Triangles);

    size_t position_accessor_id = 0;
    size_t normal_accessor_id = 0;
    beyond::optional<size_t> texture_coord_accessor_id;
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

    if (const auto itr = primitive.attributes.find("TEXCOORD_0");
        itr != primitive.attributes.end()) {
      texture_coord_accessor_id = itr->second;
    }
    // TODO: handle meshes without index accessor
    index_accessor_id = primitive.indicesAccessor.value();

    const fastgltf::Accessor& position_accessor = parsed_asset->accessors.at(position_accessor_id);
    const fastgltf::Accessor& normal_accessor = parsed_asset->accessors.at(normal_accessor_id);
    const fastgltf::Accessor& index_accessor = parsed_asset->accessors.at(index_accessor_id);

    using fastgltf::AccessorType;
    BEYOND_ENSURE(position_accessor.type == AccessorType::Vec3);
    BEYOND_ENSURE(normal_accessor.type == AccessorType::Vec3);
    BEYOND_ENSURE(index_accessor.type == AccessorType::Scalar);

    std::vector<beyond::Vec3> positions;
    positions.resize(position_accessor.count);
    fastgltf::copyFromAccessor<beyond::Vec3>(*parsed_asset, position_accessor, positions.data());

    std::vector<beyond::Vec3> normals;
    normals.resize(normal_accessor.count);
    fastgltf::copyFromAccessor<beyond::Vec3>(*parsed_asset, normal_accessor, normals.data());

    std::vector<beyond::Vec2> tex_coords;
    texture_coord_accessor_id
        .map([&](size_t id) {
          const auto& texture_coord_accessor = parsed_asset->accessors.at(id);

          BEYOND_ENSURE(texture_coord_accessor.type == AccessorType::Vec2);

          tex_coords.resize(texture_coord_accessor.count);
          fastgltf::copyFromAccessor<beyond::Vec2>(*parsed_asset, texture_coord_accessor,
                                                   tex_coords.data());
        })
        .or_else([&] { tex_coords.resize(positions.size()); });

    std::vector<std::uint32_t> indices;
    indices.resize(index_accessor.count);
    fastgltf::copyFromAccessor<std::uint32_t>(*parsed_asset, index_accessor, indices.data());

    BEYOND_ENSURE(primitive.materialIndex.has_value());
    beyond::optional<uint32_t> material_index;
    if (mesh.primitives[0].materialIndex.has_value()) {
      material_index = beyond::narrow<uint32_t>(mesh.primitives[0].materialIndex.value());
    }

    result.meshes.push_back(CPUMesh{.name = mesh.name,
                                    .material_index = material_index,
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