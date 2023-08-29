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

class ShaderCompiler {
  std::unique_ptr<struct ShaderCompilerImpl> impl_;

public:
  [[nodiscard]] auto compile_shader(const char* filename, ShaderStage stage)
      -> beyond::optional<std::vector<uint32_t>>;

  ShaderCompiler();
  ~ShaderCompiler();
  ShaderCompiler(const ShaderCompiler&) = delete;
  ShaderCompiler& operator=(const ShaderCompiler&) = delete;
};

} // namespace charlie

#endif // CHARLIE3D_SHADER_COMPILER_HPP
