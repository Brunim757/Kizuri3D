#pragma once
#include <string>
#include <stdint.h>
#include <vector>
namespace Kizuri {
enum class TexFormat : uint32_t {
  Rgba8 = 0,
  Bc1 = 1,
  Bc3 = 2,
  Bc5 = 3
};
struct TextureMipData {
  uint32_t width;
  uint32_t height;
  uint32_t rowPitch;
  std::vector<unsigned char> data;
};
struct TextureAssetData {
  std::string guid;
  uint32_t width;
  uint32_t height;
  TexFormat format;
  bool srgb;
  std::vector<TextureMipData> mips;
  bool hasSource;
  std::string sourcePath;
  uint64_t sourceHash;
  int64_t sourceTimestamp;
};
bool EncodeTextureFile(const TextureAssetData& data, const std::string& path);
bool DecodeTextureFile(const std::string& path, TextureAssetData& out);
bool EncodeTextureMemory(const TextureAssetData& data, std::vector<unsigned char>& out);
bool DecodeTextureMemory(const void* bytes, size_t size, TextureAssetData& out);
bool ParseCubeLut(const std::string& path, std::vector<unsigned char>& rgbaOut, int& sizeOut);
}