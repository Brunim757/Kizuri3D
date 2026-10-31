#pragma once

#include <caligo/core/Assert.h>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace caligo {

struct Error {
  std::string message;
  int code = 0;
};

template <typename T> class [[nodiscard]] Result {
public:
  Result(T value) : m_value(std::move(value)) {}
  Result(Error error) : m_value(std::move(error)) {}

  [[nodiscard]] bool HasValue() const noexcept {
    return std::holds_alternative<T>(m_value);
  }
  explicit operator bool() const noexcept {
    return HasValue();
  }

  [[nodiscard]] T& Value() & {
    if (T* value = std::get_if<T>(&m_value)) {
      return *value;
    }
    FailAssert("Result<T>::Value", __FILE__, __LINE__, "result has no value");
  }

  [[nodiscard]] const T& Value() const& {
    if (const T* value = std::get_if<T>(&m_value)) {
      return *value;
    }
    FailAssert("Result<T>::Value", __FILE__, __LINE__, "result has no value");
  }

  [[nodiscard]] T&& Value() && {
    if (T* value = std::get_if<T>(&m_value)) {
      return std::move(*value);
    }
    FailAssert("Result<T>::Value", __FILE__, __LINE__, "result has no value");
  }

  [[nodiscard]] Error& GetError() & {
    if (Error* error = std::get_if<Error>(&m_value)) {
      return *error;
    }
    FailAssert("Result<T>::GetError", __FILE__, __LINE__, "result has no error");
  }

  [[nodiscard]] const Error& GetError() const& {
    if (const Error* error = std::get_if<Error>(&m_value)) {
      return *error;
    }
    FailAssert("Result<T>::GetError", __FILE__, __LINE__, "result has no error");
  }

private:
  std::variant<T, Error> m_value;
};

template <> class [[nodiscard]] Result<void> {
public:
  Result() = default;
  Result(Error error) : m_error(std::move(error)) {}

  [[nodiscard]] bool HasValue() const noexcept {
    return !m_error.has_value();
  }
  explicit operator bool() const noexcept {
    return HasValue();
  }

  void Value() const {
    if (!m_error.has_value()) {
      return;
    }
    FailAssert("Result<void>::Value", __FILE__, __LINE__, "result has no value");
  }

  [[nodiscard]] Error& GetError() & {
    if (m_error.has_value()) {
      return *m_error;
    }
    FailAssert("Result<void>::GetError", __FILE__, __LINE__, "result has no error");
  }

  [[nodiscard]] const Error& GetError() const& {
    if (m_error.has_value()) {
      return *m_error;
    }
    FailAssert("Result<void>::GetError", __FILE__, __LINE__, "result has no error");
  }

private:
  std::optional<Error> m_error;
};

}
