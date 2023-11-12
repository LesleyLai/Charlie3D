#ifndef CHARLIE3D_GLTF_LOADER_HPP
#define CHARLIE3D_GLTF_LOADER_HPP

#include <filesystem>

#include "cpu_scene.hpp"

namespace charlie {

[[nodiscard]] auto load_gltf(const std::filesystem::path& file_path) -> CPUScene;

}

#endif // CHARLIE3D_GLTF_LOADER_HPP
