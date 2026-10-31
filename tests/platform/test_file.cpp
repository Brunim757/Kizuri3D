#include <caligo/platform/File.h>
#include <chrono>
#include <cstddef>
#include <doctest/doctest.h>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {

class TempDirectory {
public:
  TempDirectory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = std::filesystem::temp_directory_path() / "caligo_tests" / std::to_string(stamp);
    std::filesystem::remove_all(m_path);
    std::filesystem::create_directories(m_path);
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;

  [[nodiscard]] const std::filesystem::path& Path() const noexcept {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

}

TEST_CASE("text files round trip and create parent directories") {
  TempDirectory temp;
  const std::filesystem::path path = temp.Path() / "nested" / "hello.txt";

  const caligo::Result<void> written = caligo::platform::WriteTextFile(path, "hello\nworld");
  REQUIRE(written);
  CHECK(caligo::platform::FileExists(path));

  const caligo::Result<std::string> read = caligo::platform::ReadTextFile(path);
  REQUIRE(read);
  CHECK(read.Value() == "hello\nworld");
}

TEST_CASE("binary files round trip") {
  TempDirectory temp;
  const std::filesystem::path path = temp.Path() / "payload.bin";
  const std::vector<std::byte> data{std::byte{0x00}, std::byte{0x7F}, std::byte{0xFF}};

  const caligo::Result<void> written =
      caligo::platform::WriteBinaryFile(path, std::span<const std::byte>(data.data(), data.size()));
  REQUIRE(written);

  const caligo::Result<caligo::platform::ByteBuffer> read = caligo::platform::ReadBinaryFile(path);
  REQUIRE(read);
  REQUIRE(read.Value().size() == data.size());
  CHECK(read.Value() == data);
}

TEST_CASE("missing files return an error") {
  TempDirectory temp;
  const caligo::Result<std::string> missing =
      caligo::platform::ReadTextFile(temp.Path() / "missing.txt");

  CHECK_FALSE(missing);
  CHECK_FALSE(missing.GetError().message.empty());
}

TEST_CASE("executable directory resolves") {
  const caligo::Result<std::filesystem::path> directory = caligo::platform::ExecutableDirectory();

  REQUIRE(directory);
  CHECK_FALSE(directory.Value().empty());
  CHECK(std::filesystem::is_directory(directory.Value()));
}
