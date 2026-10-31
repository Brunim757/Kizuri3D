#pragma once

#include <caligo/core/Result.h>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace caligo::platform {

using ByteBuffer = std::vector<std::byte>;

[[nodiscard]] Result<std::string> ReadTextFile(const std::filesystem::path& path);
[[nodiscard]] Result<ByteBuffer> ReadBinaryFile(const std::filesystem::path& path);
[[nodiscard]] Result<void> WriteTextFile(const std::filesystem::path& path, std::string_view text);
[[nodiscard]] Result<void> WriteBinaryFile(const std::filesystem::path& path,
                                           std::span<const std::byte> bytes);
[[nodiscard]] bool FileExists(const std::filesystem::path& path) noexcept;
[[nodiscard]] Result<std::filesystem::path> ExecutableDirectory();

}
