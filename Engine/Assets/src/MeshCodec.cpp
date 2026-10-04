#include "Kizuri/Assets/MeshCodec.h"
#include "ZstdStream.h"
#include "kzassets_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <zstd.h>
#include <cstdio>
#include <cstring>
namespace Kizuri {
namespace {
const unsigned char kMagic[4] = { 'K', 'Z', 'M', 'H' };
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
void ComputeAABB(const std::vector<float>& positions, float outMin[3], float outMax[3]) {
  outMin[0] = 0.0f;
  outMin[1] = 0.0f;
  outMin[2] = 0.0f;
  outMax[0] = 0.0f;
  outMax[1] = 0.0f;
  outMax[2] = 0.0f;
  size_t n = positions.size() / 3;
  if (n == 0) {
    return;
  }
  outMin[0] = positions[0];
  outMin[1] = positions[1];
  outMin[2] = positions[2];
  outMax[0] = positions[0];
  outMax[1] = positions[1];
  outMax[2] = positions[2];
  for (size_t i = 1; i < n; ++i) {
    for (int c = 0; c < 3; ++c) {
      float v = positions[i * 3 + c];
      if (v < outMin[c]) {
        outMin[c] = v;
      }
      if (v > outMax[c]) {
        outMax[c] = v;
      }
    }
  }
}
bool AppendMeshFlatBuffer(const MeshAssetData& data, size_t vertexCount, flatbuffers::FlatBufferBuilder& builder) {
  std::vector<unsigned char> verts(vertexCount * 32);
  for (size_t i = 0; i < vertexCount; ++i) {
    float* dst = reinterpret_cast<float*>(&verts[i * 32]);
    dst[0] = data.positions[i * 3 + 0];
    dst[1] = data.positions[i * 3 + 1];
    dst[2] = data.positions[i * 3 + 2];
    dst[3] = data.normals[i * 3 + 0];
    dst[4] = data.normals[i * 3 + 1];
    dst[5] = data.normals[i * 3 + 2];
    dst[6] = data.uvs[i * 2 + 0];
    dst[7] = data.uvs[i * 2 + 1];
  }
  std::vector<unsigned char> idx(data.indices.size() * 4);
  std::memcpy(idx.data(), data.indices.data(), idx.size());
  auto fbVerts = builder.CreateVector(verts);
  verts.clear();
  verts.shrink_to_fit();
  auto fbIdx = builder.CreateVector(idx);
  idx.clear();
  idx.shrink_to_fit();
  std::vector<unsigned char> tans(vertexCount * 16);
  std::memcpy(tans.data(), data.tangents.data(), tans.size());
  auto fbTans = builder.CreateVector(tans);
  tans.clear();
  tans.shrink_to_fit();
  auto fbGuid = builder.CreateString(data.guid);
  auto fbSourcePath = builder.CreateString(data.sourcePath);
  std::vector<flatbuffers::Offset<KizuriAssets::MaterialSlot>> fbMats;
  for (size_t i = 0; i < data.materials.size(); ++i) {
    const MeshMaterialData& m = data.materials[i];
    auto fbName = builder.CreateString(m.name);
    auto fbTex = builder.CreateString(m.albedoTexGuid);
    auto fbNrm = builder.CreateString(m.normalTexGuid);
    auto fbOrm = builder.CreateString(m.ormTexGuid);
    fbMats.push_back(KizuriAssets::CreateMaterialSlot(builder, fbName, m.albedo[0], m.albedo[1], m.albedo[2], m.metallic, m.roughness, fbTex, fbNrm, fbOrm));
  }
  auto fbMatsVec = builder.CreateVector(fbMats);
  std::vector<KizuriAssets::MeshPart> fbParts;
  for (size_t i = 0; i < data.parts.size(); ++i) {
    KizuriAssets::MeshPart p(data.parts[i].indexOffset, data.parts[i].indexCount, data.parts[i].material);
    fbParts.push_back(p);
  }
  auto fbPartsVec = builder.CreateVectorOfStructs(fbParts);
  KizuriAssets::Vec3 bmin(data.aabbMin[0], data.aabbMin[1], data.aabbMin[2]);
  KizuriAssets::Vec3 bmax(data.aabbMax[0], data.aabbMax[1], data.aabbMax[2]);
  KizuriAssets::AABB bounds(bmin, bmax);
  auto mesh = KizuriAssets::CreateMeshAsset(builder, fbGuid, fbVerts, static_cast<uint32_t>(vertexCount), 32, fbTans, fbIdx, static_cast<uint32_t>(data.indices.size()), &bounds, fbMatsVec, fbPartsVec, data.hasSource, fbSourcePath, data.sourceHash, static_cast<int64_t>(data.sourceTimestamp));
  builder.Finish(mesh);
  return true;
}
bool ValidateMeshData(const MeshAssetData& data, size_t& vertexCount) {
  if (data.guid.empty() || data.positions.empty() || data.indices.empty()) {
    return false;
  }
  vertexCount = data.positions.size() / 3;
  if (data.normals.size() != data.positions.size() || data.uvs.size() != vertexCount * 2 || data.tangents.size() != vertexCount * 4) {
    return false;
  }
  return true;
}
bool EncodeMeshMemory(const MeshAssetData& data, std::vector<unsigned char>& out) {
  out.clear();
  size_t vertexCount = 0;
  if (!ValidateMeshData(data, vertexCount)) {
    return false;
  }
  size_t hint = vertexCount * 32 + data.indices.size() * 4 + 1024;
  flatbuffers::FlatBufferBuilder builder(hint);
  if (!AppendMeshFlatBuffer(data, vertexCount, builder)) {
    return false;
  }
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
bool DecodeMeshMemory(const void* bytes, size_t size, MeshAssetData& out) {
  out = MeshAssetData();
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
  if (!KizuriAssets::VerifyMeshAssetBuffer(verifier)) {
    return false;
  }
  const KizuriAssets::MeshAsset* mesh = KizuriAssets::GetMeshAsset(fb.data());
  if (mesh->guid() == nullptr || mesh->vertexData() == nullptr || mesh->indexData() == nullptr) {
    return false;
  }
  if (mesh->vertexStride() != 32 || mesh->vertexCount() == 0 || mesh->indexCount() == 0) {
    return false;
  }
  if (mesh->vertexData()->size() != mesh->vertexCount() * 32 || mesh->indexData()->size() != mesh->indexCount() * 4) {
    return false;
  }
  out.guid = mesh->guid()->str();
  size_t vertexCount = mesh->vertexCount();
  out.positions.resize(vertexCount * 3);
  out.normals.resize(vertexCount * 3);
  out.uvs.resize(vertexCount * 2);
  out.tangents.resize(vertexCount * 4);
  const float* vdata = reinterpret_cast<const float*>(mesh->vertexData()->data());
  for (size_t i = 0; i < vertexCount; ++i) {
    out.positions[i * 3 + 0] = vdata[i * 8 + 0];
    out.positions[i * 3 + 1] = vdata[i * 8 + 1];
    out.positions[i * 3 + 2] = vdata[i * 8 + 2];
    out.normals[i * 3 + 0] = vdata[i * 8 + 3];
    out.normals[i * 3 + 1] = vdata[i * 8 + 4];
    out.normals[i * 3 + 2] = vdata[i * 8 + 5];
    out.uvs[i * 2 + 0] = vdata[i * 8 + 6];
    out.uvs[i * 2 + 1] = vdata[i * 8 + 7];
  }
  size_t indexCount = mesh->indexCount();
  out.indices.resize(indexCount);
  std::memcpy(out.indices.data(), mesh->indexData()->data(), indexCount * 4);
  if (mesh->tangentData() != nullptr && mesh->tangentData()->size() == vertexCount * 16) {
    std::memcpy(out.tangents.data(), mesh->tangentData()->data(), vertexCount * 16);
  } else if (!GenerateTangents(out.positions, out.normals, out.uvs, out.indices, out.tangents)) {
    out = MeshAssetData();
    return false;
  }
  if (mesh->bounds() != nullptr) {
    out.aabbMin[0] = mesh->bounds()->min().x();
    out.aabbMin[1] = mesh->bounds()->min().y();
    out.aabbMin[2] = mesh->bounds()->min().z();
    out.aabbMax[0] = mesh->bounds()->max().x();
    out.aabbMax[1] = mesh->bounds()->max().y();
    out.aabbMax[2] = mesh->bounds()->max().z();
  }
  if (mesh->materials() != nullptr) {
    for (uint32_t i = 0; i < mesh->materials()->size(); ++i) {
      const KizuriAssets::MaterialSlot* m = mesh->materials()->Get(i);
      MeshMaterialData md;
      md.name = m->name() != nullptr ? m->name()->str() : "";
      md.albedo[0] = m->albedoR();
      md.albedo[1] = m->albedoG();
      md.albedo[2] = m->albedoB();
      md.metallic = m->metallic();
      md.roughness = m->roughness();
      md.albedoTexGuid = m->albedoTexGuid() != nullptr ? m->albedoTexGuid()->str() : "";
      md.normalTexGuid = m->normalTexGuid() != nullptr ? m->normalTexGuid()->str() : "";
      md.ormTexGuid = m->ormTexGuid() != nullptr ? m->ormTexGuid()->str() : "";
      out.materials.push_back(md);
    }
  }
  if (mesh->parts() != nullptr) {
    for (uint32_t i = 0; i < mesh->parts()->size(); ++i) {
      const KizuriAssets::MeshPart* p = mesh->parts()->Get(i);
      MeshPartData pd;
      pd.indexOffset = p->indexOffset();
      pd.indexCount = p->indexCount();
      pd.material = p->material();
      out.parts.push_back(pd);
    }
  }
  out.hasSource = mesh->hasSource();
  out.sourcePath = mesh->sourcePath() != nullptr ? mesh->sourcePath()->str() : "";
  out.sourceHash = mesh->sourceHash();
  out.sourceTimestamp = mesh->sourceTimestamp();
  return true;
}
bool EncodeMeshFile(const MeshAssetData& data, const std::string& path) {
  size_t vertexCount = 0;
  if (!ValidateMeshData(data, vertexCount)) {
    return false;
  }
  size_t hint = vertexCount * 32 + data.indices.size() * 4 + 1024;
  flatbuffers::FlatBufferBuilder builder(hint);
  if (!AppendMeshFlatBuffer(data, vertexCount, builder)) {
    return false;
  }
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
bool DecodeMeshFile(const std::string& path, MeshAssetData& out) {
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
  return DecodeMeshMemory(bytes.data(), bytes.size(), out);
}
}
