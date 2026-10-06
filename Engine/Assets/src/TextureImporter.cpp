#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Assets/Guid.h"
#include <stb_image.h>
#include <DirectXTex.h>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <thread>
#include <vector>
namespace Kizuri {
namespace {
bool NameHintsNormal(const std::string& name) {
  std::string lower = name;
  for (size_t i = 0; i < lower.size(); ++i) {
    lower[i] = static_cast<char>(tolower(lower[i]));
  }
  return lower.find("normal") != std::string::npos || lower.find("_nrm") != std::string::npos || lower.find("_nor") != std::string::npos || lower.find("normalmap") != std::string::npos;
}
bool NameHintsOrm(const std::string& name) {
  std::string lower = name;
  for (size_t i = 0; i < lower.size(); ++i) {
    lower[i] = static_cast<char>(tolower(lower[i]));
  }
  if (lower.find("_orm") != std::string::npos || lower.find("_arm") != std::string::npos || lower.find("_ao") != std::string::npos) {
    return true;
  }
  return lower.find("_rough") != std::string::npos && lower.find("_metal") != std::string::npos;
}
}
bool DecodeTextureRgba(const void* bytes, size_t size, std::vector<unsigned char>& rgba, int& w, int& h) {
  rgba.clear();
  w = 0;
  h = 0;
  if (bytes == nullptr || size == 0 || size > static_cast<size_t>(2147483647)) {
    return false;
  }
  int comp = 0;
  unsigned char* px = stbi_load_from_memory(static_cast<const stbi_uc*>(bytes), static_cast<int>(size), &w, &h, &comp, 4);
  if (px == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  rgba.assign(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(px);
  return true;
}
bool ImportTextureMemory(const void* bytes, size_t size, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, TexKind kind, const std::string& sourcePath, uint64_t sourceHash) {
  out = TextureAssetData();
  if (bytes == nullptr || size == 0) {
    return false;
  }
  int w = 0;
  int h = 0;
  int comp = 0;
  unsigned char* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(bytes), static_cast<int>(size), &w, &h, &comp, 4);
  if (pixels == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  bool ok = CompressTexturePixels(pixels, w, h, keepGuid, out, nameHint, kind, sourcePath, sourceHash);
  stbi_image_free(pixels);
  return ok;
}
bool CompressTexturePixels(const unsigned char* rgba, int w, int h, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, TexKind kind, const std::string& sourcePath, uint64_t sourceHash) {
  out = TextureAssetData();
  if (rgba == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  bool normal = kind == TexKind::Normal || (kind == TexKind::Auto && NameHintsNormal(nameHint));
  bool orm = kind == TexKind::Orm || (kind == TexKind::Auto && !normal && NameHintsOrm(nameHint));
  bool hasAlpha = false;
  for (int i = 0; i < w * h; ++i) {
    if (rgba[i * 4 + 3] < 255) {
      hasAlpha = true;
      break;
    }
  }
  bool srgb = !normal && !orm;
  DirectX::ScratchImage src;
  if (FAILED(src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, w, h, 1, 1))) {
    return false;
  }
  std::memcpy(src.GetPixels(), rgba, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  DirectX::ScratchImage mipChain;
  DirectX::TEX_FILTER_FLAGS filter = static_cast<DirectX::TEX_FILTER_FLAGS>(DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_FORCE_NON_WIC);
  if (srgb) {
    filter = static_cast<DirectX::TEX_FILTER_FLAGS>(filter | DirectX::TEX_FILTER_SRGB);
  }
  if (FAILED(DirectX::GenerateMipMaps(src.GetImages(), src.GetImageCount(), src.GetMetadata(), filter, 0, mipChain))) {
    return false;
  }
  DXGI_FORMAT destFormat = DXGI_FORMAT_BC1_UNORM;
  TexFormat outFormat = TexFormat::Bc1;
  if (normal) {
    destFormat = DXGI_FORMAT_BC5_UNORM;
    outFormat = TexFormat::Bc5;
  } else if (orm) {
    destFormat = hasAlpha ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
    outFormat = hasAlpha ? TexFormat::Bc3 : TexFormat::Bc1;
  } else if (hasAlpha) {
    destFormat = srgb ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    outFormat = TexFormat::Bc3;
  } else {
    destFormat = srgb ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
    outFormat = TexFormat::Bc1;
  }
  DirectX::ScratchImage compressed;
  size_t mipCount = mipChain.GetImageCount();
  const DirectX::Image* srcImgs = mipChain.GetImages();
  bool compressOk = true;
  if (mipCount <= 1) {
    if (FAILED(DirectX::Compress(srcImgs, mipCount, mipChain.GetMetadata(), destFormat, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressed))) {
      return false;
    }
  } else {
    unsigned int hwThreads = std::thread::hardware_concurrency();
    if (hwThreads < 1) {
      hwThreads = 1;
    }
    size_t threadCount = mipCount < static_cast<size_t>(hwThreads) ? mipCount : static_cast<size_t>(hwThreads);
    std::vector<DirectX::ScratchImage> parts(mipCount);
    std::vector<HRESULT> partResults(mipCount, S_OK);
    std::atomic<size_t> next(0);
    std::vector<std::thread> workers;
    for (size_t t = 0; t < threadCount; ++t) {
      workers.push_back(std::thread([&]() {
        for (;;) {
          size_t i = next.fetch_add(1);
          if (i >= mipCount) {
            break;
          }
          partResults[i] = DirectX::Compress(srcImgs[i], destFormat, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, parts[i]);
        }
      }));
    }
    for (size_t t = 0; t < workers.size(); ++t) {
      workers[t].join();
    }
    for (size_t i = 0; i < mipCount; ++i) {
      if (FAILED(partResults[i]) || parts[i].GetImageCount() == 0) {
        compressOk = false;
        break;
      }
    }
    if (compressOk) {
      DirectX::TexMetadata md = mipChain.GetMetadata();
      md.format = destFormat;
      if (FAILED(compressed.Initialize(md))) {
        compressOk = false;
      } else {
        for (size_t i = 0; i < mipCount && compressOk; ++i) {
          const DirectX::Image* dst = compressed.GetImage(i, 0, 0);
          const DirectX::Image* s = parts[i].GetImage(0, 0, 0);
          if (dst == nullptr || s == nullptr || dst->slicePitch != s->slicePitch) {
            compressOk = false;
            break;
          }
          std::memcpy(dst->pixels, s->pixels, dst->slicePitch);
        }
      }
    }
  }
  if (!compressOk) {
    return false;
  }
  out.guid = keepGuid.empty() ? GenerateGuidString() : keepGuid;
  out.width = static_cast<uint32_t>(w);
  out.height = static_cast<uint32_t>(h);
  out.format = outFormat;
  out.srgb = srgb;
  for (size_t i = 0; i < compressed.GetImageCount(); ++i) {
    const DirectX::Image* img = compressed.GetImages() + i;
    TextureMipData mip;
    mip.width = static_cast<uint32_t>(img->width);
    mip.height = static_cast<uint32_t>(img->height);
    mip.rowPitch = static_cast<uint32_t>(img->rowPitch);
    mip.data.assign(img->pixels, img->pixels + img->slicePitch);
    out.mips.push_back(mip);
  }
  if (out.mips.empty()) {
    out = TextureAssetData();
    return false;
  }
  out.hasSource = !sourcePath.empty();
  out.sourcePath = sourcePath;
  out.sourceHash = sourceHash;
  out.sourceTimestamp = 0;
  if (out.hasSource) {
    std::error_code ec;
    std::filesystem::file_time_type mtime = std::filesystem::last_write_time(sourcePath, ec);
    if (!ec) {
      out.sourceTimestamp = static_cast<int64_t>(mtime.time_since_epoch().count());
    }
  }
  return true;
}
bool ImportTextureFile(const std::string& path, const std::string& keepGuid, TextureAssetData& out) {
  out = TextureAssetData();
  if (path.empty()) {
    return false;
  }
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }
  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (sz <= 0) {
    std::fclose(fp);
    return false;
  }
  std::vector<unsigned char> bytes(static_cast<size_t>(sz));
  size_t n = std::fread(bytes.data(), 1, bytes.size(), fp);
  std::fclose(fp);
  if (n != bytes.size()) {
    return false;
  }
  uint64_t hash = Fnv1a64(bytes.data(), bytes.size());
  std::filesystem::path p(path);
  return ImportTextureMemory(bytes.data(), bytes.size(), keepGuid, out, p.filename().string(), TexKind::Auto, path, hash);
}
bool ImportTextureFileAs(const std::string& path, const std::string& keepGuid, TextureAssetData& out, TexKind kind) {
  out = TextureAssetData();
  if (path.empty()) {
    return false;
  }
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }
  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (sz <= 0) {
    std::fclose(fp);
    return false;
  }
  std::vector<unsigned char> bytes(static_cast<size_t>(sz));
  size_t n = std::fread(bytes.data(), 1, bytes.size(), fp);
  std::fclose(fp);
  if (n != bytes.size()) {
    return false;
  }
  uint64_t hash = Fnv1a64(bytes.data(), bytes.size());
  std::filesystem::path p(path);
  return ImportTextureMemory(bytes.data(), bytes.size(), keepGuid, out, p.filename().string(), kind, path, hash);
}
}
