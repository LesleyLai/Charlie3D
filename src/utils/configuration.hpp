#ifndef CHARLIE3D_CONFIGURATION_HPP
#define CHARLIE3D_CONFIGURATION_HPP

#include <any>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <beyond/utils/assert.hpp>

constexpr std::string_view CONFIG_ASSETS_PATH = "ASSETS_PATH";

// A singleton that store global configurations of the program
class Configurations {
  std::unordered_map<std::string_view, std::any> configs_;

public:
  Configurations() = default;
  ~Configurations() = default;

  [[nodiscard]] static auto instance() -> Configurations&
  {
    static Configurations s;
    return s;
  }

  template <class T> void set(std::string_view key, T value)
  {
    auto [_itr, inserted] = configs_.try_emplace(key, value);
    if (!inserted)
      beyond::panic(
          fmt::format("Configuration with key {} already exist", key));
  }

  template <class T>
  [[nodiscard]] auto try_get(std::string_view key) const -> const T*
  {
    if (auto itr = configs_.find(key); itr != configs_.end()) {
      return std::any_cast<T>(&itr->second);
    } else {
      return nullptr;
    }
  }

  template <class T>
  [[nodiscard]] auto get(std::string_view key) const -> const T&
  {
    const auto* result = try_get<T>(key);
    BEYOND_ENSURE_MSG(result != nullptr,
                      fmt::format("Cannot extract configuration \"{}\"", key));
    return *result;
  }

  Configurations(const Configurations&) = delete;
  auto operator=(const Configurations&) -> Configurations& = delete;
};

#endif // CHARLIE3D_CONFIGURATION_HPP
