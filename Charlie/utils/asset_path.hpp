#ifndef CHARLIE3D_ASSET_PATH_HPP
#define CHARLIE3D_ASSET_PATH_HPP

#include <filesystem>

namespace charlie {

[[nodiscard]] auto get_asset_path() -> std::filesystem::path;

} // namespace charlie

#endif // CHARLIE3D_ASSET_PATH_HPP
