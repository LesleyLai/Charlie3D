#include "shader_compiler.hpp"

#include "../../utils/prelude.hpp"

#include <beyond/utils/assert.hpp>
#include <beyond/utils/narrowing.hpp>
#include <beyond/utils/utils.hpp>
#include <beyond/utils/zstring_view.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <shaderc/shaderc.hpp>
#include <source_location>

#include <array>
#include <string>

#include "../../utils/configuration.hpp"

namespace charlie {

struct ShaderCompilerImpl {
  shaderc::Compiler compiler;
  std::filesystem::path shader_directory;

  ShaderCompilerImpl()
  {
    const auto asset_path =
        Configurations::instance().get<std::filesystem::path>(CONFIG_ASSETS_PATH);
    shader_directory = asset_path / "shaders";
  }
};

} // namespace charlie

namespace {

[[nodiscard]] auto read_text_file(beyond::ZStringView filename) -> std::string
{
  const std::ifstream file(filename.c_str());
  BEYOND_ENSURE_MSG(file.is_open(), fmt::format("Cannot open file {}\n", filename));
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

class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
  struct IncludeData {
    std::unique_ptr<shaderc_include_result> include_result;
    std::string filename;
    std::string content;
  };
  std::vector<IncludeData> includes_;

  auto GetInclude(const char* requested_source, shaderc_include_type type,
                  const char* requesting_source, size_t /*include_depth*/)
      -> shaderc_include_result* override
  {

    std::filesystem::path requesting_directory = requesting_source;
    requesting_directory.remove_filename();

    // TODO: handle #include <>
    BEYOND_ENSURE(type == shaderc_include_type_relative);
    const auto requested_path = canonical(requesting_directory / requested_source);

    std::string name = requested_path.string();
    std::string contents = read_text_file(name);

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

auto compile_shader_impl(charlie::ShaderCompilerImpl& shader_compiler_impl,
                         beyond::ZStringView filename, const std::string& src,
                         charlie::ShaderStage stage) -> beyond::optional<std::vector<u32>>
{
  const shaderc::Compiler& compiler = shader_compiler_impl.compiler;

  shaderc::CompileOptions compile_options{};
  compile_options.SetGenerateDebugInfo();
  compile_options.SetIncluder(std::make_unique<ShaderIncluder>());

  // compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);

  const auto shader_kind = to_shaderc_shader_kind(stage);

  const auto compilation_result =
      compiler.CompileGlslToSpv(src, shader_kind, filename.c_str(), compile_options);

  if (compilation_result.GetCompilationStatus() !=
      shaderc_compilation_status::shaderc_compilation_status_success) {
    SPDLOG_ERROR("Failed to compile {}\n{}", filename, compilation_result.GetErrorMessage());
    return beyond::nullopt;
  }

  SPDLOG_INFO("Compile {}", filename);
  return std::vector(compilation_result.begin(), compilation_result.end());
}

} // anonymous namespace

namespace charlie {

ShaderCompiler::ShaderCompiler() : impl_{std::make_unique<ShaderCompilerImpl>()} {}
ShaderCompiler::~ShaderCompiler() = default;

[[nodiscard]] auto read_spirv_binary(beyond::ZStringView filename) -> std::vector<uint32_t>
{
  std::ifstream file(filename.c_str(), std::ios::ate | std::ios::binary);

  BEYOND_ENSURE_MSG(file.is_open(), fmt::format("Cannot open file {}\n", filename));

  const auto file_size = beyond::narrow<size_t>(file.tellg());
  BEYOND_ENSURE(file_size % sizeof(uint32_t) == 0);

  std::vector<uint32_t> buffer;
  buffer.resize(file_size / 4);

  file.seekg(0);
  file.read(bit_cast<char*>(buffer.data()), narrow<std::streamsize>(file_size));

  BEYOND_ENSURE_MSG(file.good(), fmt::format("Error when reading {}\n", filename));

  return buffer;
}

auto ShaderCompiler::compile_shader_from_file(beyond::ZStringView shader_path,
                                              ShaderCompilationOptions options)
    -> beyond::optional<ShaderCompilationResult>
{
  // const auto shader_path = canonical(impl_->shader_directory / filename);

  //  auto spirv_path = shader_path;
  //  spirv_path.replace_extension("spv");

  // const bool has_old_version = exists(spirv_path);
  const auto shader_src = read_text_file(shader_path);
  return compile_shader_impl(*impl_, shader_path, shader_src, options.stage)
      .map([&](std::vector<u32>&& data) {
        //        std::ofstream spirv_file{spirv_path, std::ios::out | std::ios::binary};
        //        BEYOND_ENSURE_MSG(spirv_file.is_open(),
        //                          fmt::format("Failed to open {}", spirv_path.string()));
        //        spirv_file.write(std::bit_cast<const char*>(data.data()),
        //                         narrow<std::streamsize>(data.size() * sizeof(uint32_t)));

        return ShaderCompilationResult{
            .spirv = BEYOND_MOV(data),
        };
      });
}

} // namespace charlie