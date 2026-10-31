#include <caligo/core/Math.h>
#include <doctest/doctest.h>

TEST_CASE("math vector operations") {
  const caligo::math::Vec3 left{1.0f, 2.0f, 3.0f};
  const caligo::math::Vec3 right{4.0f, 5.0f, 6.0f};

  const caligo::math::Vec3 sum = caligo::math::Add(left, right);
  CHECK(sum.x == doctest::Approx(5.0f));
  CHECK(sum.y == doctest::Approx(7.0f));
  CHECK(sum.z == doctest::Approx(9.0f));

  const caligo::math::Vec3 difference = caligo::math::Subtract(right, left);
  CHECK(difference.x == doctest::Approx(3.0f));
  CHECK(difference.y == doctest::Approx(3.0f));
  CHECK(difference.z == doctest::Approx(3.0f));

  const caligo::math::Vec3 scaled = caligo::math::Scale(left, 2.0f);
  CHECK(scaled.x == doctest::Approx(2.0f));
  CHECK(scaled.y == doctest::Approx(4.0f));
  CHECK(scaled.z == doctest::Approx(6.0f));

  CHECK(caligo::math::Dot(left, right) == doctest::Approx(32.0f));

  const caligo::math::Vec3 cross = caligo::math::Cross(left, right);
  CHECK(cross.x == doctest::Approx(-3.0f));
  CHECK(cross.y == doctest::Approx(6.0f));
  CHECK(cross.z == doctest::Approx(-3.0f));
}

TEST_CASE("math length and normalization") {
  const caligo::math::Vec3 value{3.0f, 4.0f, 0.0f};
  CHECK(caligo::math::Length(value) == doctest::Approx(5.0f));

  const caligo::math::Vec3 normalized = caligo::math::Normalize(value);
  CHECK(normalized.x == doctest::Approx(0.6f));
  CHECK(normalized.y == doctest::Approx(0.8f));
  CHECK(normalized.z == doctest::Approx(0.0f));
  CHECK(caligo::math::Length(normalized) == doctest::Approx(1.0f));

  const caligo::math::Vec3 zero = caligo::math::Normalize(caligo::math::Vec3{0.0f, 0.0f, 0.0f});
  CHECK(zero.x == 0.0f);
  CHECK(zero.y == 0.0f);
  CHECK(zero.z == 0.0f);
}

TEST_CASE("math scalar helpers") {
  CHECK(caligo::math::Clamp(5.0f, 0.0f, 1.0f) == doctest::Approx(1.0f));
  CHECK(caligo::math::Clamp(-5.0f, 0.0f, 1.0f) == doctest::Approx(0.0f));
  CHECK(caligo::math::Clamp(0.5f, 0.0f, 1.0f) == doctest::Approx(0.5f));
  CHECK(caligo::math::Lerp(0.0f, 10.0f, 0.25f) == doctest::Approx(2.5f));
  CHECK(caligo::math::Radians(180.0f) == doctest::Approx(caligo::math::kPi));
  CHECK(caligo::math::Degrees(caligo::math::kPi) == doctest::Approx(180.0f));
}

TEST_CASE("math matrix identity and multiplication") {
  const caligo::math::Mat4 identity = caligo::math::Identity();
  CHECK(identity._11 == doctest::Approx(1.0f));
  CHECK(identity._22 == doctest::Approx(1.0f));
  CHECK(identity._33 == doctest::Approx(1.0f));
  CHECK(identity._44 == doctest::Approx(1.0f));
  CHECK(identity._12 == doctest::Approx(0.0f));

  const caligo::math::Mat4 product = caligo::math::Multiply(identity, identity);
  CHECK(product._11 == doctest::Approx(1.0f));
  CHECK(product._44 == doctest::Approx(1.0f));
  CHECK(product._12 == doctest::Approx(0.0f));
}
