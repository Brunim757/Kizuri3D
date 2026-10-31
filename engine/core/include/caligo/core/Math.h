#pragma once

#include <DirectXMath.h>
#include <cmath>

namespace caligo::math {

using Vec3 = DirectX::XMFLOAT3;
using Mat4 = DirectX::XMFLOAT4X4;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kEpsilon = 1.0e-6f;

[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

[[nodiscard]] constexpr float Lerp(float from, float to, float factor) noexcept {
  return from + (to - from) * factor;
}

[[nodiscard]] constexpr float Radians(float degrees) noexcept {
  return degrees * (kPi / 180.0f);
}

[[nodiscard]] constexpr float Degrees(float radians) noexcept {
  return radians * (180.0f / kPi);
}

[[nodiscard]] inline Vec3 Add(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(
      &result, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&left), DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline Vec3 Subtract(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&left),
                                                            DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline Vec3 Scale(const Vec3& value, float factor) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&value), factor));
  return result;
}

[[nodiscard]] inline float Dot(const Vec3& left, const Vec3& right) noexcept {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&left), DirectX::XMLoadFloat3(&right)));
}

[[nodiscard]] inline Vec3 Cross(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&left),
                                                          DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline float Length(const Vec3& value) noexcept {
  return std::sqrt(Dot(value, value));
}

[[nodiscard]] inline Vec3 Normalize(const Vec3& value) noexcept {
  const float length = Length(value);
  if (length <= kEpsilon) {
    return Vec3{0.0f, 0.0f, 0.0f};
  }
  return Scale(value, 1.0f / length);
}

[[nodiscard]] inline Mat4 Identity() noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixIdentity());
  return result;
}

[[nodiscard]] inline Mat4 Multiply(const Mat4& left, const Mat4& right) noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&left),
                                                              DirectX::XMLoadFloat4x4(&right)));
  return result;
}

}
