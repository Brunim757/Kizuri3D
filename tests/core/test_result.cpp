#include <caligo/core/Result.h>
#include <doctest/doctest.h>
#include <string>

TEST_CASE("result carries a value") {
  caligo::Result<int> result = 42;
  CHECK(result.HasValue());
  CHECK(result);
  CHECK(result.Value() == 42);
}

TEST_CASE("result carries an error") {
  caligo::Result<int> result = caligo::Error{"failure", 9};
  CHECK_FALSE(result.HasValue());
  CHECK_FALSE(result);
  CHECK(result.GetError().code == 9);
  CHECK(result.GetError().message == "failure");
}

TEST_CASE("void result reports success and failure") {
  caligo::Result<void> success;
  CHECK(success.HasValue());
  CHECK(success);
  success.Value();

  caligo::Result<void> failure = caligo::Error{"bad", 2};
  CHECK_FALSE(failure.HasValue());
  CHECK_FALSE(failure);
  CHECK(failure.GetError().code == 2);
}
