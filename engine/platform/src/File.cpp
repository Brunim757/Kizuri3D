#include <caligo/platform/File.h>
#include <cstring>
#include <format>
#include <fstream>
#include <system_error>
#include <windows.h>

namespace caligo::platform {

namespace {

Error FileError(const std::filesystem::path& path, std::string_view action) {
  return Error{std::format("{}: {}", action, path.string()), 1};
}

std::string WindowsErrorMessage(DWORD error) {
  return std::error_code(static_cast<int>(error), std::system_category()).message();
}

}

Result<std::string> ReadTextFile(const std::filesystem::path& path) {
  Result<ByteBuffer> bytes = ReadBinaryFile(path);
  if (!bytes) {
    return bytes.GetError();
  }

  const ByteBuffer& buffer = bytes.Value();
  std::string text(buffer.size(), '\0');
  if (!buffer.empty()) {
    std::memcpy(text.data(), buffer.data(), buffer.size());
  }
  return text;
}

Result<ByteBuffer> ReadBinaryFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return FileError(path, "could not open file");
  }

  file.seekg(0, std::ios::end);
  const std::streampos end = file.tellg();
  if (end == std::streampos(-1)) {
    return FileError(path, "could not determine file size");
  }

  const std::streamoff size = static_cast<std::streamoff>(end);
  if (size < 0) {
    return FileError(path, "invalid file size");
  }

  file.seekg(0, std::ios::beg);
  if (!file) {
    return FileError(path, "could not seek to the beginning");
  }

  ByteBuffer bytes(static_cast<size_t>(size));
  if (!bytes.empty()) {
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file) {
      return FileError(path, "could not read file");
    }
  }
  return bytes;
}

Result<void> WriteTextFile(const std::filesystem::path& path, std::string_view text) {
  return WriteBinaryFile(path, std::as_bytes(std::span<const char>(text.data(), text.size())));
}

Result<void> WriteBinaryFile(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  std::error_code error;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return Error{std::format("could not create directory {}: {}", path.parent_path().string(),
                               error.message()),
                   1};
    }
  }

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return FileError(path, "could not open file for writing");
  }

  if (!bytes.empty()) {
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) {
      return FileError(path, "could not write file");
    }
  }
  return {};
}

bool FileExists(const std::filesystem::path& path) noexcept {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error);
}

Result<std::filesystem::path> ExecutableDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0) {
    return Error{std::format("GetModuleFileNameW failed: {}", WindowsErrorMessage(GetLastError())),
                 1};
  }
  if (static_cast<size_t>(length) >= buffer.size()) {
    return Error{"executable path is too long", 2};
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

}
