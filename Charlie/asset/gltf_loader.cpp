#include "gltf_loader.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <beyond/math/matrix.hpp>
#include <beyond/math/rotor.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/types/optional_conversion.hpp>
#include <beyond/utils/narrowing.hpp>

#include <beyond/concurrency/thread_pool.hpp>

#include <latch>

using beyond::Mat4;
using beyond::Vec3;
using beyond::Vec4;

using charlie::i32;
using charlie::narrow;
using charlie::u32;
using charlie::usize;

namespace fastgltf {

template <>
struct ElementTraits<beyond::Vec2> : ElementTraitsBase<beyond::Vec2, AccessorType::Vec2, float> {};

template <>
struct ElementTraits<beyond::Vec3> : ElementTraitsBase<beyond::Vec3, AccessorType::Vec3, float> {};

template <>
struct ElementTraits<beyond::Point3>
    : ElementTraitsBase<beyond::Point3, AccessorType::Vec3, float> {};

template <>
struct ElementTraits<beyond::Vec4> : ElementTraitsBase<beyond::Vec4, AccessorType::Vec4, float> {};

} // namespace fastgltf

namespace {

template <typename T> auto to_beyond(fastgltf::OptionalWithFlagValue<T> opt) -> beyond::optional<T>
{
  if (opt.has_value()) { return opt.value(); }
  return beyond::nullopt;
}

[[nodiscard]]
auto parse_gltf_from_file(const std::filesystem::path& file_path)
    -> fastgltf::Expected<fastgltf::Asset>
{
  fastgltf::Parser parser;

  using fastgltf::Options;

  fastgltf::GltfDataBuffer data;
  {
    ZoneScopedN("Load GLTF from File");
    data.loadFromFile(file_path);
  }

  const bool is_glb = file_path.extension() == ".glb";
  const auto directory = file_path.parent_path();

  {
    ZoneScopedN("Parse GLTF");
    return is_glb ? parser.loadBinaryGLTF(&data, directory,
                                          Options::LoadGLBBuffers | Options::LoadExternalBuffers)
                  : parser.loadGLTF(&data, directory,
                                    Options::LoadGLBBuffers | Options::LoadExternalBuffers);
  }
}

auto to_cpu_texture(const fastgltf::Texture& texture) -> charlie::CPUTexture
{
  return {.name = std::string{texture.name},
          .image_index = narrow<uint32_t>(texture.imageIndex.value()),
          .sampler_index = to_beyond(texture.samplerIndex).map(narrow<uint32_t, size_t>)};
}

[[nodiscard]] auto from_fastgltf(fastgltf::AlphaMode alpha_mode) -> charlie::AlphaMode
{
  switch (alpha_mode) {
  case fastgltf::AlphaMode::Opaque:
    return charlie::AlphaMode::opaque;
  case fastgltf::AlphaMode::Mask:
    return charlie::AlphaMode::mask;
  case fastgltf::AlphaMode::Blend:
    return charlie::AlphaMode::blend;
  }
  return charlie::AlphaMode::opaque;
}

auto to_cpu_material(const fastgltf::Material& material) -> charlie::CPUMaterial
{
  static constexpr auto get_texture_index = [](const fastgltf::TextureInfo& texture) -> uint32_t {
    return narrow<uint32_t>(texture.textureIndex);
  };

  const beyond::optional<uint32_t> albedo_texture_index =
      beyond::from_std(material.pbrData.baseColorTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> normal_texture_index =
      beyond::from_std(material.normalTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> metallic_roughness_texture_index =
      beyond::from_std(material.pbrData.metallicRoughnessTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> occlusion_texture_index =
      beyond::from_std(material.occlusionTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> emissive_texture_index =
      beyond::from_std(material.emissiveTexture.transform(get_texture_index));

  const auto alpha_mode = from_fastgltf(material.alphaMode);

  return charlie::CPUMaterial{
      .base_color_factor = beyond::Vec4{material.pbrData.baseColorFactor},
      .metallic_factor = material.pbrData.metallicFactor,
      .roughness_factor = material.pbrData.roughnessFactor,
      .albedo_texture_index = albedo_texture_index,
      .normal_texture_index = normal_texture_index,
      .metallic_roughness_texture_index = metallic_roughness_texture_index,
      .occlusion_texture_index = occlusion_texture_index,
      .emissive_texture_index = emissive_texture_index,
      .emissive_factor = Vec3{material.emissiveFactor},
      .alpha_mode = alpha_mode,
      .alpha_cutoff = material.alphaCutoff,
  };
};

[[nodiscard]] auto load_raw_image_data(const std::filesystem::path& gltf_directory,
                                       const fastgltf::Asset& asset,
                                       const fastgltf::Image& image) -> charlie::CPUImage
{
  return std::visit(
      [&](const auto& data) -> charlie::CPUImage {
        using DataType = std::remove_cvref_t<decltype(data)>;
        if constexpr (std::is_same_v<DataType, fastgltf::sources::URI>) {
          std::filesystem::path file_path = data.uri.fspath().is_absolute()
                                                ? data.uri.fspath()
                                                : gltf_directory / data.uri.fspath();
          file_path = std::filesystem::canonical(file_path);

          const auto name = image.name.empty() ? file_path.string() : std::string{image.name};
          return charlie::load_image_from_file(file_path, name);
        } else if constexpr (std::is_same_v<DataType, fastgltf::sources::Vector>) {
          using enum fastgltf::MimeType;
          BEYOND_ENSURE(data.mimeType == JPEG || data.mimeType == PNG ||
                        data.mimeType == GltfBuffer);
          return charlie::load_image_from_memory(data.bytes, std::string{image.name});
        } else if constexpr (std::is_same_v<DataType, fastgltf::sources::BufferView>) {
          using enum fastgltf::MimeType;
          BEYOND_ENSURE(data.mimeType == JPEG || data.mimeType == PNG ||
                        data.mimeType == GltfBuffer);
          const std::size_t buffer_view_index = data.bufferViewIndex;
          const auto& buffer_view = asset.bufferViews.at(buffer_view_index);
          const auto& buffer = asset.buffers.at(buffer_view.bufferIndex);

          return std::visit(
              [&](const auto& arg) -> charlie::CPUImage {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(arg)>,
                                             fastgltf::sources::Vector>) {
                  std::span<const uint8_t> bytes(arg.bytes.data() + buffer_view.byteOffset,
                                                 buffer_view.byteLength);
                  return charlie::load_image_from_memory(bytes, std::string{image.name});
                } else {
                  beyond::panic("Should not happen!");
                }
              },
              buffer.data);

        } else {
          beyond::panic("Unsupported image data format!");
        }
      },
      image.data);
}

// Calculate whether each node has no parent
[[nodiscard]] auto calculate_is_root(std::span<const fastgltf::Node> nodes) -> std::vector<bool>
{
  std::vector<bool> is_root(nodes.size(), true);
  for (const auto& [i, node] : std::views::enumerate(nodes)) {
    for (charlie::usize child_index : node.children) { is_root.at(child_index) = false; }
  }
  return is_root;
}

[[nodiscard]] auto get_node_transform(const fastgltf::Node& node) -> Mat4
{
  if (const auto* transform_ptr = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform);
      transform_ptr != nullptr) {
    return Mat4::from_span(*transform_ptr);
  } else {
    const auto transform = std::get<fastgltf::Node::TRS>(node.transform);
    const Vec3 translation{transform.translation};

    const auto rotation = beyond::Rotor3::from_quaternion(
        transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
    const auto rotation_mat = Mat4{rotation.to_mat3()};

    const Vec3 scale{transform.scale};

    // T * R * S
    return beyond::translate(translation) * rotation_mat * beyond::scale(scale);
  }
}

void add_node(const fastgltf::Node& node, std::span<const fastgltf::Node> inputs,
              charlie::Nodes& output, std::vector<i32>& parent_indices, i32 parent_index = -1)
{
  output.names.push_back(std::string{node.name});
  parent_indices.push_back(parent_index);

  const auto local_transform = get_node_transform(node);
  output.local_transforms.push_back(local_transform);

  const i32 mesh_index = to_beyond(node.meshIndex).map(narrow<i32, usize>).value_or(-1);
  output.mesh_indices.push_back(mesh_index);

  BEYOND_ENSURE(output.names.size() == output.local_transforms.size());
  BEYOND_ENSURE(output.names.size() == output.mesh_indices.size());

  const i32 new_node_index = narrow<i32>(output.names.size() - 1);
  for (auto child_index : node.children) {
    add_node(inputs[child_index], inputs, output, parent_indices, new_node_index);
  }
}

[[nodiscard]]
auto populate_global_transforms(std::span<const i32> parent_indices,
                                std::span<const Mat4> local_transforms) -> std::vector<Mat4>
{
  // generate global transforms
  std::vector<Mat4> global_transforms(local_transforms.begin(), local_transforms.end());

  BEYOND_ENSURE(local_transforms.size() == parent_indices.size());
  for (usize i = 0; i < local_transforms.size(); ++i) {
    // If there is a parent
    const i32 parent_index = parent_indices[i];
    BEYOND_ENSURE_MSG(parent_index < narrow<i32>(i),
                      "Scene graph nodes are not topologically sorted");

    const Mat4 local_transform = local_transforms[i];
    if (parent_index >= 0) {
      const Mat4 parent_global_transform = global_transforms[parent_index];
      global_transforms[i] = parent_global_transform * local_transform;
    }
  }
  return global_transforms;
}

[[nodiscard]]
auto populate_nodes(std::span<const fastgltf::Node> asset_nodes) -> charlie::Nodes
{
  charlie::Nodes result;

  std::vector<i32> parent_indices;

  const usize node_count = asset_nodes.size();

  result.names.reserve(node_count);
  result.local_transforms.reserve(node_count);
  result.mesh_indices.reserve(node_count);

  const std::vector<bool> node_is_root = calculate_is_root(asset_nodes);
  BEYOND_ENSURE(node_is_root.size() == node_count);
  for (const auto [i, node] : std::views::enumerate(asset_nodes)) {
    if (node_is_root[i]) { add_node(node, asset_nodes, result, parent_indices); }
  }

  // generate global transforms
  result.global_transforms = populate_global_transforms(parent_indices, result.local_transforms);
  return result;
}

// Convert mesh from fastgltf format to charlie::CPUMesh
auto convert_meshes(const fastgltf::Asset& asset) -> std::vector<charlie::CPUMesh>
{
  ZoneScopedN("Convert Meshes");

  std::vector<charlie::CPUMesh> meshes;
  meshes.reserve(asset.meshes.size());

  for (const auto& mesh : asset.meshes) {
    ZoneScopedN("Convert Mesh");
    ZoneText(mesh.name.data(), mesh.name.size());

    // Each gltf primitive is treated as a submesh
    std::vector<charlie::CPUSubmesh> submeshes;
    submeshes.reserve(mesh.primitives.size());
    for (const auto& primitive : mesh.primitives) {
      BEYOND_ENSURE(primitive.type == fastgltf::PrimitiveType::Triangles);
      constexpr auto find_attribute_id = [](const fastgltf::Primitive& primitive,
                                            std::string_view name) -> beyond::optional<usize> {
        if (const auto itr = primitive.findAttribute(name); itr != primitive.attributes.end()) {
          return itr->second;
        } else {
          return beyond::nullopt;
        }
      };

      const usize position_accessor_id =
          find_attribute_id(primitive, "POSITION").expect("Mesh misses POSITION attribute!");
      const usize normal_accessor_id =
          find_attribute_id(primitive, "NORMAL").expect("Mesh misses POSITION attribute!");
      const beyond::optional<usize> tangent_accessor_id = find_attribute_id(primitive, "TANGENT");
      const beyond::optional<usize> texture_coord_accessor_id =
          find_attribute_id(primitive, "TEXCOORD_0");

      // TODO: handle meshes without index accessor
      const usize index_accessor_id = primitive.indicesAccessor.value();

      const fastgltf::Accessor& position_accessor = asset.accessors.at(position_accessor_id);
      const fastgltf::Accessor& normal_accessor = asset.accessors.at(normal_accessor_id);
      const fastgltf::Accessor& index_accessor = asset.accessors.at(index_accessor_id);

      using fastgltf::AccessorType;
      BEYOND_ENSURE(position_accessor.type == AccessorType::Vec3);
      BEYOND_ENSURE(normal_accessor.type == AccessorType::Vec3);
      BEYOND_ENSURE(index_accessor.type == AccessorType::Scalar);

      std::vector<beyond::Point3> positions;
      positions.resize(position_accessor.count);
      fastgltf::copyFromAccessor<beyond::Point3>(asset, position_accessor, positions.data());

      std::vector<beyond::Vec3> normals;
      normals.resize(normal_accessor.count);
      fastgltf::copyFromAccessor<beyond::Vec3>(asset, normal_accessor, normals.data());

      std::vector<beyond::Vec2> tex_coords;
      texture_coord_accessor_id
          .map([&](size_t id) {
            const auto& texture_coord_accessor = asset.accessors.at(id);
            BEYOND_ENSURE(texture_coord_accessor.type == AccessorType::Vec2);

            tex_coords.resize(texture_coord_accessor.count);
            fastgltf::copyFromAccessor<beyond::Vec2>(asset, texture_coord_accessor,
                                                     tex_coords.data());
          })
          .or_else([&] { tex_coords.resize(positions.size()); });

      std::vector<beyond::Vec4> tangents;
      tangent_accessor_id.map([&](size_t id) {
        const auto& tangent_accessor = asset.accessors.at(id);
        BEYOND_ENSURE(tangent_accessor.type == AccessorType::Vec4);

        tangents.resize(tangent_accessor.count);
        fastgltf::copyFromAccessor<beyond::Vec4>(asset, tangent_accessor, tangents.data());
      });
      if (tangents.empty()) { tangents.resize(positions.size()); }

      // BEYOND_ENSURE(index_accessor.componentType == fastgltf::ComponentType::UnsignedInt);
      std::vector<u32> indices;
      indices.resize(index_accessor.count);
      fastgltf::copyFromAccessor<u32>(asset, index_accessor, indices.data());

      const beyond::optional<u32> material_index =
          to_beyond(primitive.materialIndex).map(beyond::narrow<u32, usize>);

      std::vector<charlie::Vertex> vertex_buffer(positions.size());
      for (size_t i = 0; i < positions.size(); ++i) {
        // Interleave normal, uv, and tangents
        vertex_buffer[i] = {.normal = charlie::vec3_to_oct(normals[i]),
                            .tex_coords = tex_coords[i],
                            .tangents = tangents[i]};
      }

      submeshes.push_back(charlie::CPUSubmesh{.material_index = material_index,
                                              .positions = BEYOND_MOV(positions),
                                              .vertices = BEYOND_MOV(vertex_buffer),
                                              .indices = BEYOND_MOV(indices)});
    }

    meshes.push_back(
        charlie::CPUMesh{.name = std::string{mesh.name}, .submeshes = BEYOND_MOV(submeshes)});
  }
  return meshes;
}

} // anonymous namespace

namespace charlie {

static beyond::ThreadPool bg_thread_pool;

[[nodiscard]] auto load_gltf(const std::filesystem::path& file_path) -> CPUScene
{
  ZoneScoped;

  auto maybe_asset = parse_gltf_from_file(file_path);
  if (const auto error = maybe_asset.error(); error != fastgltf::Error::None) {
    beyond::panic(fmt::format("Error while loading {}: {}", file_path.string(),
                              fastgltf::getErrorMessage(error)));
  }
  fastgltf::Asset& asset = maybe_asset.get();

  CPUScene result;
  result.nodes = populate_nodes(asset.nodes);

  const auto gltf_directory = file_path.parent_path();
  std::latch image_loading_latch{narrow<ptrdiff_t>(asset.images.size())};

  result.images.resize(asset.images.size());
  for (const auto& [i, image] : std::views::enumerate(asset.images)) {
    bg_thread_pool.async([&, i]() {
      result.images[i] = load_raw_image_data(gltf_directory, asset, image);
      image_loading_latch.count_down();
    });
  }

  {
    ZoneScopedN("Convert Textures");
    result.textures.reserve(asset.textures.size());
    std::ranges::transform(asset.textures, std::back_inserter(result.textures), to_cpu_texture);
  }

  {
    ZoneScopedN("Convert Materials");
    result.materials.reserve(asset.materials.size());
    std::ranges::transform(asset.materials, std::back_inserter(result.materials), to_cpu_material);
  }

  result.meshes = convert_meshes(asset);

  if (const auto error = fastgltf::validate(asset); error != fastgltf::Error::None) {
    SPDLOG_ERROR("GLTF validation error {} from {}", fastgltf::getErrorName(error),
                 file_path.string());
  }

  image_loading_latch.wait();

  return result;
}

} // namespace charlie