#include "Kizuri/Assets/TexCodec.h"
#include "ZstdStream.h"
#include "kztex_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <zstd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace Kizuri {
namespace {
const unsigned char kMagic[4] = { 'K', 'Z', 'T', 'X' };
const uint32_t kVersion = 1;
void WriteU32(unsigned char* dst, uint32_t v) {
  dst[0] = static_cast<unsigned char>(v & 0xFF);
  dst[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
  dst[2] = static_cast<unsigned char>((v >> 16) & 0xFF);
  dst[3] = static_cast<unsigned char>((v >> 24) & 0xFF);
}
uint32_t ReadU32(const unsigned char* src) {
  return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}
void WriteU64(unsigned char* dst, uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    dst[i] = static_cast<unsigned char>((v >> (i * 8)) & 0xFF);
  }
}
uint64_t ReadU64(const unsigned char* src) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v |= static_cast<uint64_t>(src[i]) << (i * 8);
  }
  return v;
}
}
bool EncodeTextureMemory(const TextureAssetData& data, std::vector<unsigned char>& out) {
  out.clear();
  if (data.guid.empty() || data.width == 0 || data.height == 0 || data.mips.empty()) {
    return false;
  }
  flatbuffers::FlatBufferBuilder builder(4096);
  std::vector<flatbuffers::Offset<KizuriAssets::MipLevel>> fbMips;
  for (size_t i = 0; i < data.mips.size(); ++i) {
    const TextureMipData& mip = data.mips[i];
    if (mip.data.empty()) {
      return false;
    }
    auto fbData = builder.CreateVector(mip.data);
    fbMips.push_back(KizuriAssets::CreateMipLevel(builder, mip.width, mip.height, mip.rowPitch, fbData));
  }
  auto fbMipsVec = builder.CreateVector(fbMips);
  auto fbGuid = builder.CreateString(data.guid);
  auto fbSourcePath = builder.CreateString(data.sourcePath);
  auto tex = KizuriAssets::CreateTextureAsset(builder, fbGuid, data.width, data.height, static_cast<uint32_t>(data.mips.size()), static_cast<uint32_t>(data.format), data.srgb, data.hasSource, fbSourcePath, data.sourceHash, static_cast<int64_t>(data.sourceTimestamp), fbMipsVec);
  builder.Finish(tex);
  size_t fbSize = builder.GetSize();
  size_t bound = ZSTD_compressBound(fbSize);
  std::vector<unsigned char> comp(bound);
  size_t csize = ZSTD_compress(comp.data(), bound, builder.GetBufferPointer(), fbSize, 3);
  if (ZSTD_isError(csize)) {
    return false;
  }
  out.resize(16 + csize);
  out[0] = kMagic[0];
  out[1] = kMagic[1];
  out[2] = kMagic[2];
  out[3] = kMagic[3];
  WriteU32(&out[4], kVersion);
  WriteU64(&out[8], static_cast<uint64_t>(fbSize));
  std::memcpy(&out[16], comp.data(), csize);
  return true;
}
bool DecodeTextureMemory(const void* bytes, size_t size, TextureAssetData& out) {
  out = TextureAssetData();
  if (bytes == nullptr || size < 16) {
    return false;
  }
  const unsigned char* src = static_cast<const unsigned char*>(bytes);
  if (src[0] != kMagic[0] || src[1] != kMagic[1] || src[2] != kMagic[2] || src[3] != kMagic[3]) {
    return false;
  }
  if (ReadU32(&src[4]) != kVersion) {
    return false;
  }
  uint64_t fbSize = ReadU64(&src[8]);
  if (fbSize == 0 || fbSize > 1024 * 1024 * 1024) {
    return false;
  }
  std::vector<unsigned char> fb(static_cast<size_t>(fbSize));
  size_t dsize = ZSTD_decompress(fb.data(), fb.size(), src + 16, size - 16);
  if (ZSTD_isError(dsize) || dsize != fb.size()) {
    return false;
  }
  flatbuffers::Verifier verifier(fb.data(), fb.size());
  if (!KizuriAssets::VerifyTextureAssetBuffer(verifier)) {
    return false;
  }
  const KizuriAssets::TextureAsset* tex = KizuriAssets::GetTextureAsset(fb.data());
  if (tex->guid() == nullptr || tex->mips() == nullptr || tex->mips()->size() == 0) {
    return false;
  }
  out.guid = tex->guid()->str();
  out.width = tex->width();
  out.height = tex->height();
  out.format = static_cast<TexFormat>(tex->format());
  out.srgb = tex->srgb();
  for (uint32_t i = 0; i < tex->mips()->size(); ++i) {
    const KizuriAssets::MipLevel* mip = tex->mips()->Get(i);
    TextureMipData md;
    md.width = mip->width();
    md.height = mip->height();
    md.rowPitch = mip->rowPitch();
    if (mip->data() == nullptr || mip->data()->size() == 0) {
      return false;
    }
    md.data.assign(mip->data()->data(), mip->data()->data() + mip->data()->size());
    out.mips.push_back(md);
  }
  out.hasSource = tex->hasSource();
  out.sourcePath = tex->sourcePath() != nullptr ? tex->sourcePath()->str() : "";
  out.sourceHash = tex->sourceHash();
  out.sourceTimestamp = tex->sourceTimestamp();
  return true;
}
bool EncodeTextureFile(const TextureAssetData& data, const std::string& path) {
  if (data.guid.empty() || data.width == 0 || data.height == 0 || data.mips.empty()) {
    return false;
  }
  size_t total = 0;
  for (size_t i = 0; i < data.mips.size(); ++i) {
    total += data.mips[i].data.size();
  }
  size_t hint = total + 1024;
  flatbuffers::FlatBufferBuilder builder(hint);
  std::vector<flatbuffers::Offset<KizuriAssets::MipLevel>> fbMips;
  for (size_t i = 0; i < data.mips.size(); ++i) {
    const TextureMipData& mip = data.mips[i];
    if (mip.data.empty()) {
      return false;
    }
    auto fbData = builder.CreateVector(mip.data);
    fbMips.push_back(KizuriAssets::CreateMipLevel(builder, mip.width, mip.height, mip.rowPitch, fbData));
  }
  auto fbMipsVec = builder.CreateVector(fbMips);
  auto fbGuid = builder.CreateString(data.guid);
  auto fbSourcePath = builder.CreateString(data.sourcePath);
  auto tex = KizuriAssets::CreateTextureAsset(builder, fbGuid, data.width, data.height, static_cast<uint32_t>(data.mips.size()), static_cast<uint32_t>(data.format), data.srgb, data.hasSource, fbSourcePath, data.sourceHash, static_cast<int64_t>(data.sourceTimestamp), fbMipsVec);
  builder.Finish(tex);
  size_t fbSize = builder.GetSize();
  FILE* fp = std::fopen(path.c_str(), "wb");
  if (fp == nullptr) {
    return false;
  }
  unsigned char header[16];
  header[0] = kMagic[0];
  header[1] = kMagic[1];
  header[2] = kMagic[2];
  header[3] = kMagic[3];
  WriteU32(&header[4], kVersion);
  WriteU64(&header[8], static_cast<uint64_t>(fbSize));
  bool ok = std::fwrite(header, 1, sizeof(header), fp) == sizeof(header);
  if (ok) {
    ok = StreamZstdToFile(builder.GetBufferPointer(), fbSize, fp);
  }
  std::fclose(fp);
  if (!ok) {
    std::remove(path.c_str());
  }
  return ok;
}
bool DecodeTextureFile(const std::string& path, TextureAssetData& out) {
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
  return DecodeTextureMemory(bytes.data(), bytes.size(), out);
}
bool ParseCubeLut(const std::string& path, std::vector<unsigned char>& rgbaOut, int& sizeOut) {
  rgbaOut.clear();
  sizeOut = 0;
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }
  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (sz <= 0 || sz > 16 * 1024 * 1024) {
    std::fclose(fp);
    return false;
  }
  std::string text(static_cast<size_t>(sz), '\0');
  size_t n = std::fread(text.data(), 1, text.size(), fp);
  std::fclose(fp);
  if (n != text.size()) {
    return false;
  }
  int lutSize = 0;
  float dmin[3] = { 0.0f, 0.0f, 0.0f };
  float dmax[3] = { 1.0f, 1.0f, 1.0f };
  std::vector<float> vals;
  vals.reserve(32768 * 3);
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    std::string line = text.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = eol == std::string::npos ? text.size() : eol + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
      ++start;
    }
    if (start >= line.size() || line[start] == '#') {
      continue;
    }
    std::string body = line.substr(start);
    if (body.compare(0, 11, "LUT_3D_SIZE") == 0) {
      lutSize = std::atoi(body.c_str() + 11);
    } else if (body.compare(0, 10, "DOMAIN_MIN") == 0) {
      std::sscanf(body.c_str() + 10, "%f %f %f", &dmin[0], &dmin[1], &dmin[2]);
    } else if (body.compare(0, 10, "DOMAIN_MAX") == 0) {
      std::sscanf(body.c_str() + 10, "%f %f %f", &dmax[0], &dmax[1], &dmax[2]);
    } else if (body.compare(0, 5, "TITLE") == 0 || body.compare(0, 13, "LUT_1D_SIZE") == 0) {
      continue;
    } else {
      float r = 0.0f;
      float g = 0.0f;
      float b = 0.0f;
      if (std::sscanf(body.c_str(), "%f %f %f", &r, &g, &b) == 3) {
        vals.push_back(r);
        vals.push_back(g);
        vals.push_back(b);
      }
    }
  }
  if (lutSize < 2 || lutSize > 64) {
    return false;
  }
  if (vals.size() != static_cast<size_t>(lutSize) * static_cast<size_t>(lutSize) * static_cast<size_t>(lutSize) * 3) {
    return false;
  }
  rgbaOut.resize(vals.size() / 3 * 4);
  for (size_t i = 0; i < vals.size() / 3; ++i) {
    for (int c = 0; c < 3; ++c) {
      float denom = dmax[c] - dmin[c];
      float v = denom != 0.0f ? (vals[i * 3 + c] - dmin[c]) / denom : vals[i * 3 + c];
      if (v < 0.0f) {
        v = 0.0f;
      }
      if (v > 1.0f) {
        v = 1.0f;
      }
      rgbaOut[i * 4 + c] = static_cast<unsigned char>(v * 255.0f + 0.5f);
    }
    rgbaOut[i * 4 + 3] = 255;
  }
  sizeOut = lutSize;
  return true;
}
}
