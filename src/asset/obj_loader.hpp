#ifndef CHARLIE3D_OBJ_LOADER_HPP
#define CHARLIE3D_OBJ_LOADER_HPP

#include <filesystem>

#include "cpu_scene.hpp"

namespace charlie {

[[nodiscard]] auto load_obj(const std::filesystem::path& file_path) -> CPUScene;

} // namespace charlie

#endif // CHARLIE3D_OBJ_LOADER_HPP
