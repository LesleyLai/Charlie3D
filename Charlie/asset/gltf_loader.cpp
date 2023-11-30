#include "gltf_loader.hpp"

#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

#include <beyond/math/matrix.hpp>
#include <beyond/math/transform.hpp>
#include <beyond/types/optional.hpp>
#include <beyond/types/optional_conversion.hpp>
#include <beyond/utils/narrowing.hpp>

#include "../utils/thread_pool.hpp"

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

auto to_cpu_texture(const fastgltf::Texture& texture) -> charlie::CPUTexture
{
  return {.name = std::string{texture.name},
          .image_index = beyond::narrow<uint32_t>(texture.imageIndex.value()),
          .sampler_index = to_beyond(texture.samplerIndex).map(beyond::narrow<uint32_t, size_t>)};
}

auto to_cpu_material(const fastgltf::Material& material) -> charlie::CPUMaterial
{
  static constexpr auto get_texture_index = [](const fastgltf::TextureInfo& texture) -> uint32_t {
    return beyond::narrow<uint32_t>(texture.textureIndex);
  };

  const beyond::optional<uint32_t> albedo_texture_index =
      beyond::from_std(material.pbrData.baseColorTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> normal_texture_index =
      beyond::from_std(material.normalTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> metallic_roughness_texture_index =
      beyond::from_std(material.pbrData.metallicRoughnessTexture.transform(get_texture_index));

  const beyond::optional<uint32_t> occlusion_texture_index =
      beyond::from_std(material.occlusionTexture.transform(get_texture_index));

  return charlie::CPUMaterial{
      .base_color_factor = {material.pbrData.baseColorFactor[0],
                            material.pbrData.baseColorFactor[1],
                            material.pbrData.baseColorFactor[2],
                            material.pbrData.baseColorFactor[3]},
      .metallic_factor = material.pbrData.metallicFactor,
      .roughness_factor = material.pbrData.roughnessFactor,
      .albedo_texture_index = albedo_texture_index,
      .normal_texture_index = normal_texture_index,
      .metallic_roughness_texture_index = metallic_roughness_texture_index,
      .occlusion_texture_index = occlusion_texture_index,
  };
};

[[nodiscard]] auto load_raw_image_data(const std::filesystem::path& gltf_directory,
                                       const fastgltf::Asset& asset, const fastgltf::Image& image)
    -> charlie::CPUImage
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
    const auto transform = std::get<fastgltf::Node::TRS>(node.transform);
    const beyond::Vec3 translation{transform.translation[0], transform.translation[1],
                                   transform.translation[2]};
    //    const beyond::Quat rotation{transform.rotation[3], transform.rotation[0],
    //    transform.rotation[1],
    //                                transform.rotation[2]};
    const beyond::Vec3 scale{transform.scale[0], transform.scale[1], transform.scale[2]};

    // TODO: proper handle TRS
    return beyond::translate(translation) * beyond::scale(scale);
  }
}

//[[nodiscard]] auto calculate_bounding_box(std::span<const beyond::Point3> positions)
//    -> beyond::AABB3
//{
//
//  beyond::Point3 min{1e20f, 1e20f, 1e20f};
//  beyond::Point3 max{-1e20f, -1e20f, -1e20f};
//  for (const auto& position : positions) {
//    min = beyond::min(min, position);
//    max = beyond::max(max, position);
//  }
//
//  return beyond::AABB3{min, max};
//}

[[nodiscard]] auto load_gltf(const std::filesystem::path& file_path) -> CPUScene
{
  ZoneScoped;

  ThreadPool io_thread_pool{"IO Thread Pool"};

  fastgltf::Parser parser;

  fastgltf::GltfDataBuffer data;
  data.loadFromFile(file_path);

  using fastgltf::Options;

  auto maybe_asset = parser.loadGLTF(&data, file_path.parent_path(),
                                     Options::LoadGLBBuffers | Options::LoadExternalBuffers);
  if (const auto error = maybe_asset.error(); error != fastgltf::Error::None) {
    beyond::panic(fmt::format("Error while loading {}: {}", file_path.string(),
                              fastgltf::getErrorMessage(error)));
  }
  fastgltf::Asset& asset = maybe_asset.get();

  CPUScene result;

  for (auto& node : asset.nodes) {
    const auto transform = get_node_transform(node);
    const i32 mesh_index = to_beyond(node.meshIndex).map(beyond::narrow<i32, usize>).value_or(-1);
    result.objects.push_back({.mesh_index = mesh_index});
    result.local_transforms.push_back(transform);
  }

  result.images.resize(asset.images.size());

  std::vector<Task<>> tasks;
  for (usize i = 0; i < asset.images.size(); ++i) {
    tasks.emplace_back([&asset](charlie::ThreadPool& scheduler,
                                std::filesystem::path gltf_directory, const fastgltf::Image& image,
                                charlie::CPUImage& output) -> Task<> {
      co_await scheduler.schedule();
      output = load_raw_image_data(gltf_directory, asset, image);
    }(io_thread_pool, file_path.parent_path(), asset.images[i], result.images[i]));
    io_thread_pool.enqueue(tasks.back());
  }

  result.textures.reserve(asset.textures.size());
  std::ranges::transform(asset.textures, std::back_inserter(result.textures), to_cpu_texture);

  result.materials.reserve(asset.materials.size());
  std::ranges::transform(asset.materials, std::back_inserter(result.materials), to_cpu_material);

  for (const auto& mesh : asset.meshes) {
    // Each gltf primitive is treated as an own mesh

    std::vector<CPUSubmesh> submeshes;
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

      // BEYOND_ENSURE(index_accessor.componentType == fastgltf::ComponentType::UnsignedInt);
      std::vector<u32> indices;
      indices.resize(index_accessor.count);
      fastgltf::copyFromAccessor<u32>(asset, index_accessor, indices.data());

      const beyond::optional<u32> material_index =
          to_beyond(primitive.materialIndex).map(beyond::narrow<u32, usize>);

      submeshes.push_back(CPUSubmesh{.material_index = material_index,
                                     .positions = std::move(positions),
                                     .normals = std::move(normals),
                                     .uv = std::move(tex_coords),
                                     .tangents = std::move(tangents),
                                     .indices = std::move(indices)});
    }

    result.meshes.push_back(
        CPUMesh{.name = std::string{mesh.name}, .submeshes = std::move(submeshes)});
  }

  if (const auto error = fastgltf::validate(asset); error != fastgltf::Error::None) {
    SPDLOG_ERROR("GLTF validation error {} from {}", fastgltf::getErrorName(error),
                 file_path.string());
  }

  // TODO: don't hard code floor here
  //  {
  //    result.materials.emplace_back();
  //    result.meshes.push_back(CPUMesh{
  //        .name = "Floor",
  //        .material_index = result.materials.size() - 1,
  //        .positions =
  //            {
  //                Point3{0.5f, 0.0f, 0.5f},   // top right
  //                Point3{0.5f, 0.0f, -0.5f},  // bottom right
  //                Point3{-0.5f, 0.0f, -0.5f}, // bottom left
  //                Point3{-0.5f, 0.0f, 0.5f}   // top left
  //            },
  //        .normals = {Vec3{0, 1, 0}, Vec3{0, 1, 0}, Vec3{0, 1, 0}, Vec3{0, 1, 0}},
  //        .uv = {Vec2{0, 0}, Vec2{0, 1}, Vec2{1, 0}, Vec2{1, 1}},
  //        .tangents = {Vec4{-1, 0, 0, 1}, Vec4{-1, 0, 0, 1}, Vec4{-1, 0, 0, 1}, Vec4{-1, 0, 0,
  //        1}}, .indices = {0, 1, 3, 1, 2, 3},
  //    });
  //    result.objects.push_back(CPURenderObject{
  //        .mesh_index = narrow<i32>(result.meshes.size() - 1),
  //    });
  //    result.local_transforms.push_back(beyond::translate(0.0f, -1.0f, 0.0f) *
  //                                      beyond::scale(5.0f, 1.f, 5.0f));
  //  }

  io_thread_pool.wait();

  SPDLOG_INFO("GLTF loaded from {}", file_path.string());
  return result;
}

} // namespace charlie