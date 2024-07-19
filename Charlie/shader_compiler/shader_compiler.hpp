#ifndef CHARLIE3D_SHADER_COMPILER_HPP
#define CHARLIE3D_SHADER_COMPILER_HPP

#include "beyond/types/optional.hpp"
#include "beyond/utils/zstring_view.hpp"
#include <memory>
#include <vector>

namespace charlie {

enum class ShaderStage : std::uint8_t {
  vertex,
  fragment,
  compute,
  geometry,
  tess_control,
  tess_evaluation,

  // mesh shader
  task,
  mesh,

  // ray tracing
  raygen,
  any_hit,
  closest_hit,
  miss,
  intersection,
  callable,
};

struct ShaderCompilationOptions {
  ShaderStage stage;
};

struct [[nodiscard]] ShaderCompilationResult {
  std::vector<uint32_t> spirv;
  std::vector<std::string> include_files; // paths for all #include files
};

class ShaderCompiler {
  std::unique_ptr<struct ShaderCompilerImpl> impl_;

public:
  [[nodiscard]] auto compile_shader_from_file(beyond::ZStringView shader_path,
                                              ShaderCompilationOptions options)
      -> beyond::optional<ShaderCompilationResult>;

  ShaderCompiler();
  ~ShaderCompiler();
  ShaderCompiler(const ShaderCompiler&) = delete;
  auto operator=(const ShaderCompiler&) -> ShaderCompiler& = delete;
};

// Read existing spirv binary from a file
[[nodiscard]] auto read_spirv_binary(beyond::ZStringView filename) -> std::vector<uint32_t>;

} // namespace charlie

#endif // CHARLIE3D_SHADER_COMPILER_HPP
