#pragma once
#include "Kizuri/Assets/TexCodec.h"
#include <string>
namespace Kizuri {
enum class TexKind {
  Auto = 0,
  Color = 1,
  Normal = 2,
  Orm = 3
};
bool ImportTextureFile(const std::string& path, const std::string& keepGuid, TextureAssetData& out);
bool ImportTextureFileAs(const std::string& path, const std::string& keepGuid, TextureAssetData& out, TexKind kind);
bool ImportTextureMemory(const void* bytes, size_t size, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, TexKind kind, const std::string& sourcePath, uint64_t sourceHash);
bool CompressTexturePixels(const unsigned char* rgba, int w, int h, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, TexKind kind, const std::string& sourcePath, uint64_t sourceHash);