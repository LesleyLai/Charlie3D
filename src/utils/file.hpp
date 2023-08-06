#ifndef CHARLIE3D_FILE_HPP
#define CHARLIE3D_FILE_HPP

#include <beyond/types/optional.hpp>
#include <filesystem>
#include <type_traits>

// filesystem related utilities

namespace charlie {

template <typename Fn>
  requires std::is_invocable_r_v<bool, Fn, std::filesystem::path>
auto upward_directory_find(const std::filesystem::path& from, Fn condition)
    -> beyond::optional<std::filesystem::path>
{
  for (auto directory_path = from; directory_path != from.root_path();
       directory_path = directory_path.parent_path()) {
    if (condition(directory_path)) { return directory_path; }
  }
  return beyond::nullopt;
}

auto locate_asset_path(const std::filesystem::path& exe_directory_path)
    -> beyond::optional<std::filesystem::path>
{
  using std::filesystem::path;
  const auto append_asset = [](const path& path) { return path / "assets"; };
  const auto parent_path = upward_directory_find(
      exe_directory_path, [&](const std::filesystem::path& path) {
        const auto assets_path = append_asset(path);
        return exists(assets_path) && is_directory(assets_path);
      });
  return parent_path.map([&](const path& path) { return append_asset(path); });
}

} // namespace charlie

#endif // CHARLIE3D_FILE_HPP
