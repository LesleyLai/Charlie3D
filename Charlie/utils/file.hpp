#ifndef CHARLIE3D_FILE_HPP
#define CHARLIE3D_FILE_HPP

#include <filesystem>
#include <optional>
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

} // namespace charlie

#endif // CHARLIE3D_FILE_HPP
