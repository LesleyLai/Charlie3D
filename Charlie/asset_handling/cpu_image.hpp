#ifndef CHARLIE3D_CPU_IMAGE_HPP
#define CHARLIE3D_CPU_IMAGE_HPP

#include <filesystem>
#include <span>

namespace charlie {

struct CPUImage {
  std::string name;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t components = 0;
  std::unique_ptr<uint8_t[]> data;
};

[[nodiscard]] auto load_image_from_file(const std::filesystem::path& path, std::string filepath)
    -> CPUImage;

[[nodiscard]] auto load_image_from_memory(std::span<const uint8_t> bytes, std::string image_name)
    -> CPUImage;

} // namespace charlie

#endif // CHARLIE3D_CPU_IMAGE_HPP
