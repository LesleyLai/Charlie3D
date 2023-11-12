#ifndef CHARLIE3D_STRING_MAP_HPP
#define CHARLIE3D_STRING_MAP_HPP

#include <beyond/utils/zstring_view.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace charlie {

struct StringHash {
  using is_transparent = void;
  [[nodiscard]] auto operator()(const char* txt) const -> size_t
  {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] auto operator()(std::string_view txt) const -> size_t
  {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] auto operator()(const std::string& txt) const -> size_t
  {
    return std::hash<std::string>{}(txt);
  }
  [[nodiscard]] auto operator()(beyond::ZStringView txt) const -> size_t
  {
    return std::hash<std::string_view>{}(txt);
  }
};

template <typename T>
using StringHashMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

} // namespace charlie

#endif // CHARLIE3D_STRING_MAP_HPP
