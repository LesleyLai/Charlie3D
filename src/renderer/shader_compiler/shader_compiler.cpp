#include "shader_compiler.hpp"

#include <beyond/utils/assert.hpp>
#include <beyond/utils/narrowing.hpp>
#include <beyond/utils/utils.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <shaderc/shaderc.hpp>
#include <source_location>

#include "../../utils/configuration.hpp"
#include "beyond/utils/bit_cast.hpp"

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

[[nodiscard]] auto read_text_file(const char* filename) -> std::string
{
  const std::ifstream file(filename);
  if (!file.is_open()) beyond::panic(fmt::format("Cannot open file {}\n", filename));
  std::stringstream buffer;
  buffer << file.rdbuf();

  if (!file.good()) { beyond::panic(fmt::format("Error when reading {}\n", filename)); }

  return buffer.str();
}

[[nodiscard]] auto read_spirv_binary(const std::string& filename) -> std::vector<uint32_t>
{
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) { beyond::panic(fmt::format("Failed to open file {}", filename)); }

  const auto file_size = beyond::narrow<size_t>(file.tellg());
  BEYOND_ENSURE(file_size % sizeof(uint32_t) == 0);

  std::vector<uint32_t> buffer;
  buffer.resize(file_size / 4);

  file.seekg(0);
  file.read(beyond::bit_cast<char*>(buffer.data()), beyond::narrow<std::streamsize>(file_size));

  if (!file.good()) {
    // Blah
  }

  return buffer;
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

auto compile_shader_impl(charlie::ShaderCompilerImpl& shader_compiler_impl,
                         const std::string& filename, const std::string& src,
                         charlie::ShaderStage stage) -> beyond::optional<std::vector<uint32_t>>
{
  const shaderc::Compiler& compiler = shader_compiler_impl.compiler;
  shaderc::CompileOptions compile_options{};
  compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);

  const auto shader_kind = to_shaderc_shader_kind(stage);

  const auto compilation_result =
      compiler.CompileGlslToSpv(src, shader_kind, filename.c_str(), compile_options);

  if (compilation_result.GetCompilationStatus() !=
      shaderc_compilation_status::shaderc_compilation_status_success) {
    SPDLOG_ERROR("{}", "Failed to compile {}\n{}", filename, compilation_result.GetErrorMessage());
    return beyond::nullopt;
  }

  SPDLOG_INFO("Compile {}", filename);
  return std::vector(compilation_result.begin(), compilation_result.end());
}

} // anonymous namespace

namespace charlie {

ShaderCompiler::ShaderCompiler() : impl_{std::make_unique<ShaderCompilerImpl>()} {}
ShaderCompiler::~ShaderCompiler() = default;

auto ShaderCompiler::compile_shader(const char* filename, ShaderStage stage)
    -> beyond::optional<std::vector<uint32_t>>
{
  const auto shader_path = canonical(impl_->shader_directory / filename);

  auto spirv_path = shader_path;
  spirv_path.replace_extension("spv");

  const bool has_old_version = exists(spirv_path);
  if (has_old_version && last_write_time(spirv_path) > last_write_time(shader_path)) {
    SPDLOG_INFO("Load {}", spirv_path.string());
    return read_spirv_binary(spirv_path.string());
  } else {
    const auto shader_filename = shader_path.string();
    const auto shader_src = read_text_file(shader_filename.c_str());
    auto data = compile_shader_impl(*impl_, shader_filename, shader_src, stage);
    if (data) {
      std::ofstream spirv_file{spirv_path, std::ios::out | std::ios::binary};
      if (!spirv_file.is_open()) {
        beyond::panic(fmt::format("Failed to open {}", spirv_path.string()));
      }
      spirv_file.write(beyond::bit_cast<const char*>(data.value().data()),
                       data.value().size() * sizeof(uint32_t));
    } else if (has_old_version) {
      SPDLOG_INFO("Fallback to old version {}", spirv_path.string());
      return read_spirv_binary(spirv_path.string());
    }

    return data;
  }
}

} // namespace charlie