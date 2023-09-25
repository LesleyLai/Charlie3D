#include "cpu_image.hpp"

#include <cstdint>
#include <memory>

#include <beyond/utils/assert.hpp>
#include <beyond/utils/narrowing.hpp>

#include <stb_image.h>

namespace charlie {

[[nodiscard]] auto load_image_from_file(const std::filesystem::path& file_path,
                                        const char* image_name) -> CPUImage
{
  int width{}, height{}, components{};
  uint8_t* pixels =
      stbi_load(file_path.string().c_str(), &width, &height, &components, STBI_rgb_alpha);
  BEYOND_ENSURE(pixels != nullptr);

  return charlie::CPUImage{
      .name = image_name,
      .width = beyond::narrow<uint32_t>(width),
      .height = beyond::narrow<uint32_t>(height),
      .compoments = beyond::narrow<uint32_t>(components),
      .data = std::unique_ptr<uint8_t[]>(pixels),
  };
}

[[nodiscard]] auto load_image_from_memory(std::span<const uint8_t> bytes, const char* image_name)
    -> CPUImage
{
  int width{}, height{}, components{};
  uint8_t* pixels = stbi_load_from_memory(bytes.data(), beyond::narrow<int>(bytes.size()), &width,
                                          &height, &components, STBI_rgb_alpha);
  BEYOND_ENSURE(pixels != nullptr);

  return charlie::CPUImage{
      .name = image_name,
      .width = beyond::narrow<uint32_t>(width),
      .height = beyond::narrow<uint32_t>(height),
      .compoments = beyond::narrow<uint32_t>(components),
      .data = std::unique_ptr<uint8_t[]>(pixels),
  };
}

} // namespace charlie