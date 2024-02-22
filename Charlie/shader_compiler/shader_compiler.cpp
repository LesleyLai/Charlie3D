#include "shader_compiler.hpp"

#include "../utils/prelude.hpp"

#include "beyond/utils/assert.hpp"
#include "beyond/utils/narrowing.hpp"
#include "beyond/utils/utils.hpp"
#include "beyond/utils/zstring_view.hpp"

#include <spdlog/spdlog.h>

#include "shaderc/shaderc.hpp"
#include <filesystem>
#include <fstream>
#include <source_location>

#include <array>
#include <string>

#include "../utils/asset_path.hpp"

namespace charlie {

struct ShaderCompilerImpl {
  shaderc::Compiler compiler;
  std::filesystem::path shader_directory;

  ShaderCompilerImpl()
  {
    const auto asset_path = get_asset_path();
    shader_directory = asset_path / "shaders";
  }
};

} // namespace charlie

namespace {

[[nodiscard]] auto read_text_file(beyond::ZStringView filename) -> std::string
{
  const std::ifstream file(filename.c_str());
  BEYOND_ENSURE_MSG(file.is_open(), fmt::format("Error: Cannot open file {}!\n", filename));

  std::stringstream buffer;
  buffer << file.rdbuf();

  BEYOND_ENSURE_MSG(file.good(), fmt::format("Error when reading {}\n", filename));

  return buffer.str();
}

[[nodiscard]] auto to_shaderc_shader_kind(charlie::ShaderStage stage)
{
  using enum charlie::ShaderStage;
  switch (stage) {
  case vertex:
    return shaderc_vertex_shader;
  case fragment:
    return shaderc_fragment_shader;
  }
  BEYOND_UNREACHABLE();
}

struct IncludeData {
  std::unique_ptr<shaderc_include_result> include_result;
  std::string filename;
  std::string content;
};

class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
  std::vector<IncludeData>& includes_;

public:
  ShaderIncluder(beyond::Ref<std::vector<IncludeData>> includes) : includes_{includes.get()} {}

private:
  auto GetInclude(const char* requested_source, shaderc_include_type /*type*/,
                  const char* requesting_source, size_t /*include_depth*/)
      -> shaderc_include_result* override
  {

    std::filesystem::path requesting_directory = requesting_source;
    requesting_directory.remove_filename();

    const auto requested_path =
        std::filesystem::weakly_canonical(requesting_directory / requested_source);

    std::string name;
    std::string contents;
    if (std::filesystem::exists(requested_path)) {
      name = requested_path.string();
      contents = read_text_file(name);
    }

    auto& result = includes_.emplace_back(std::make_unique_for_overwrite<shaderc_include_result>(),
                                          name, contents);
    *result.include_result = shaderc_include_result{
        .source_name = result.filename.data(),
        .source_name_length = result.filename.size(),
        .content = result.content.data(),
        .content_length = result.content.size(),
    };

    // fmt::print("{} includes {}\n", requesting_source, name);

    return result.include_result.get();
  }

  void ReleaseInclude(shaderc_include_result*) override {}
};

} // anonymous namespace

namespace charlie {

ShaderCompiler::ShaderCompiler() : impl_{std::make_unique<ShaderCompilerImpl>()} {}
ShaderCompiler::~ShaderCompiler() = default;

//[[nodiscard]] auto read_spirv_binary(beyond::ZStringView filename) -> std::vector<uint32_t>
//{
//  std::ifstream file(filename.c_str(), std::ios::ate | std::ios::binary);
//
//  BEYOND_ENSURE_MSG(file.is_open(), fmt::format("Cannot open file {}\n", filename));
//
//  const auto file_size = beyond::narrow<size_t>(file.tellg());
//  BEYOND_ENSURE(file_size % sizeof(uint32_t) == 0);
//
//  std::vector<uint32_t> buffer;
//  buffer.resize(file_size / 4);
//
//  file.seekg(0);
//  file.read(bit_cast<char*>(buffer.data()), narrow<std::streamsize>(file_size));
//
//  BEYOND_ENSURE_MSG(file.good(), fmt::format("Error when reading {}\n", filename));
//
//  return buffer;
//}

auto ShaderCompiler::compile_shader_from_file(beyond::ZStringView shader_path,
                                              ShaderCompilationOptions options)
    -> beyond::optional<ShaderCompilationResult>
{
  //  auto spirv_path = shader_path;
  //  spirv_path.replace_extension("spv");

  // const bool has_old_version = exists(spirv_path);

  const auto src = read_text_file(shader_path);
  const shaderc::Compiler& compiler = impl_->compiler;

  shaderc::CompileOptions compile_options{};
  compile_options.SetGenerateDebugInfo();

  std::vector<IncludeData> includes;

  compile_options.SetIncluder(std::make_unique<ShaderIncluder>(beyond::ref(includes)));

  // compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);

  const auto shader_kind = to_shaderc_shader_kind(options.stage);

  const auto compilation_result =
      compiler.CompileGlslToSpv(src, shader_kind, shader_path.c_str(), compile_options);

  if (compilation_result.GetCompilationStatus() !=
      shaderc_compilation_status::shaderc_compilation_status_success) {
    SPDLOG_ERROR("Failed to compile {}\n{}", shader_path, compilation_result.GetErrorMessage());
    return beyond::nullopt;
  }

  SPDLOG_INFO("Compiled {}", shader_path);
  std::vector<std::string> include_files;
  std::ranges::transform(includes, std::back_inserter(include_files), &IncludeData::filename);

  return charlie::ShaderCompilationResult{
      .spirv = std::vector(compilation_result.begin(), compilation_result.end()),
      .include_files = BEYOND_MOV(include_files)};
}

} // namespace charlie