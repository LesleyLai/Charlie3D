#ifndef CHARLIE3D_SHADER_COMPILER_HPP
#define CHARLIE3D_SHADER_COMPILER_HPP

#include <beyond/types/optional.hpp>
#include <memory>
#include <vector>

namespace charlie {

enum class ShaderStage {
  vertex,
  fragment,
};

struct ShaderCompilationOptions {
  ShaderStage stage;
};

struct [[nodiscard]] ShaderCompilationResult {
  std::vector<uint32_t> spirv;
  bool reuse_existing_spirv = false; // Whether we loaded an existing spirv or performed compilation
};

class ShaderCompiler {
  std::unique_ptr<struct ShaderCompilerImpl> impl_;

public:
  [[nodiscard]] auto compile_shader_from_file(const char* filename,
                                              ShaderCompilationOptions options)
      -> beyond::optional<ShaderCompilationResult>;

  ShaderCompiler();
  ~ShaderCompiler();
  ShaderCompiler(const ShaderCompiler&) = delete;
  ShaderCompiler& operator=(const ShaderCompiler&) = delete;
};

} // namespace charlie

#endif // CHARLIE3D_SHADER_COMPILER_HPP
