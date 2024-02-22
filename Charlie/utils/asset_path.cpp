#include "asset_path.hpp"

#include <beyond/utils/assets_folder_locator.hpp>

#include "file.hpp"

namespace {

auto locate_asset_path() -> beyond::optional<std::filesystem::path>
{
  using std::filesystem::path;
  const auto append_asset = [](const path& path) { return path / "assets"; };
  const auto parent_path = charlie::upward_directory_find(
      std::filesystem::current_path(), [&](const std::filesystem::path& path) {
        const auto assets_path = append_asset(path);
        return exists(assets_path) && is_directory(assets_path);
      });
  return parent_path.map([&](const path& path) { return append_asset(path); });
}

} // namespace

namespace charlie {

[[nodiscard]] auto get_asset_path() -> std::filesystem::path
{
  static auto asset_path = locate_asset_path().expect("Cannot find assets folder!");

  return asset_path;
}

} // namespace charlie