#ifndef CHARLIE3D_REQUIRED_FIELD_HPP
#define CHARLIE3D_REQUIRED_FIELD_HPP

namespace vkh {

template <typename T> struct RequiredField {
  T value;
  explicit(false) RequiredField(T v) : value{v} {}
};

} // namespace vkh

#endif // CHARLIE3D_REQUIRED_FIELD_HPP
