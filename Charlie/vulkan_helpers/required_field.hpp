#ifndef CHARLIE3D_REQUIRED_FIELD_HPP
#define CHARLIE3D_REQUIRED_FIELD_HPP

namespace vkh {

template <typename T> struct RequiredField {
  T value;
  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit(false) RequiredField(T v) : value{v} {}
  explicit(false) operator T() const // NOLINT(google-explicit-constructor)
  {
    return value;
  }
};

} // namespace vkh

#endif // CHARLIE3D_REQUIRED_FIELD_HPP
