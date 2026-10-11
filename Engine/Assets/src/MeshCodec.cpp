#include "Kizuri/Assets/MeshCodec.h"
#include "ZstdStream.h"
#include "kzassets_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <meshoptimizer.h>
#include <zstd.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
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
  bool hasUv2 = data.uv2s.size() == vertexCount * 2;
  std::vector<unsigned char> verts(vertexCount * 40);
  for (size_t i = 0; i < vertexCount; ++i) {
    float* dst = reinterpret_cast<float*>(&verts[i * 40]);
    dst[0] = data.positions[i * 3 + 0];
    dst[1] = data.positions[i * 3 + 1];
    dst[2] = data.positions[i * 3 + 2];
    dst[3] = data.normals[i * 3 + 0];
    dst[4] = data.normals[i * 3 + 1];
    dst[5] = data.normals[i * 3 + 2];
    dst[6] = data.uvs[i * 2 + 0];
    dst[7] = data.uvs[i * 2 + 1];
    dst[8] = hasUv2 ? data.uv2s[i * 2 + 0] : 0.0f;
    dst[9] = hasUv2 ? data.uv2s[i * 2 + 1] : 0.0f;
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
    auto fbHgt = builder.CreateString(m.heightTexGuid);
    auto fbEmi = builder.CreateString(m.emissiveTexGuid);
    auto fbSpc = builder.CreateString(m.specularTexGuid);
    auto fbSpcC = builder.CreateString(m.specularColorTexGuid);
    auto fbCc = builder.CreateString(m.clearcoatTexGuid);
    auto fbCcR = builder.CreateString(m.clearcoatRoughnessTexGuid);
    auto fbCcN = builder.CreateString(m.clearcoatNormalTexGuid);
    auto fbShC = builder.CreateString(m.sheenColorTexGuid);
    auto fbShR = builder.CreateString(m.sheenRoughnessTexGuid);
    auto fbTr = builder.CreateString(m.transmissionTexGuid);
    auto fbTh = builder.CreateString(m.thicknessTexGuid);
    auto fbIr = builder.CreateString(m.iridescenceTexGuid);
    auto fbIrT = builder.CreateString(m.iridescenceThicknessTexGuid);
    auto fbAn = builder.CreateString(m.anisotropyTexGuid);
    std::vector<KizuriAssets::UvXform> fbXf;
    for (int s = 0; s < 5; ++s) {
      fbXf.push_back(KizuriAssets::UvXform(m.xforms[s].offsetU, m.xforms[s].offsetV, m.xforms[s].scaleU, m.xforms[s].scaleV, m.xforms[s].rotation, m.xforms[s].uvSet));
    }
    auto fbXfVec = builder.CreateVectorOfStructs(fbXf);
    KizuriAssets::MaterialSlotBuilder msb(builder);
    msb.add_name(fbName);
    msb.add_albedoR(m.albedo[0]);
    msb.add_albedoG(m.albedo[1]);
    msb.add_albedoB(m.albedo[2]);
    msb.add_alpha(m.alpha);
    msb.add_metallic(m.metallic);
    msb.add_roughness(m.roughness);
    msb.add_albedoTexGuid(fbTex);
    msb.add_normalTexGuid(fbNrm);
    msb.add_ormTexGuid(fbOrm);
    msb.add_heightTexGuid(fbHgt);
    msb.add_emissiveR(m.emissive[0]);
    msb.add_emissiveG(m.emissive[1]);
    msb.add_emissiveB(m.emissive[2]);
    msb.add_emissiveStrength(m.emissiveStrength);
    msb.add_emissiveTexGuid(fbEmi);
    msb.add_unlit(m.unlit);
    msb.add_doubleSided(m.doubleSided);
    msb.add_alphaMode(m.alphaMode);
    msb.add_alphaCutoff(m.alphaCutoff);
    msb.add_xforms(fbXfVec);
    msb.add_ior(m.ior);
    msb.add_hasSpecular(m.hasSpecular);
    msb.add_specularR(m.specularColor[0]);
    msb.add_specularG(m.specularColor[1]);
    msb.add_specularB(m.specularColor[2]);
    msb.add_specularStrength(m.specularStrength);
    msb.add_specularTexGuid(fbSpc);
    msb.add_specularColorTexGuid(fbSpcC);
    msb.add_clearcoat(m.clearcoat);
    msb.add_clearcoatRoughness(m.clearcoatRoughness);
    msb.add_clearcoatTexGuid(fbCc);
    msb.add_clearcoatRoughnessTexGuid(fbCcR);
    msb.add_clearcoatNormalTexGuid(fbCcN);
    msb.add_sheenColorR(m.sheenColor[0]);
    msb.add_sheenColorG(m.sheenColor[1]);
    msb.add_sheenColorB(m.sheenColor[2]);
    msb.add_sheenRoughness(m.sheenRoughness);
    msb.add_sheenColorTexGuid(fbShC);
    msb.add_sheenRoughnessTexGuid(fbShR);
    msb.add_transmission(m.transmission);
    msb.add_transmissionTexGuid(fbTr);
    msb.add_thickness(m.thickness);
    msb.add_thicknessTexGuid(fbTh);
    msb.add_attenuationR(m.attenuationColor[0]);
    msb.add_attenuationG(m.attenuationColor[1]);
    msb.add_attenuationB(m.attenuationColor[2]);
    msb.add_attenuationDistance(m.attenuationDistance);
    msb.add_iridescence(m.iridescence);
    msb.add_iridescenceIor(m.iridescenceIor);
    msb.add_iridescenceThicknessMin(m.iridescenceThicknessMin);
    msb.add_iridescenceThicknessMax(m.iridescenceThicknessMax);
    msb.add_iridescenceTexGuid(fbIr);
    msb.add_iridescenceThicknessTexGuid(fbIrT);
    msb.add_anisotropy(m.anisotropy);
    msb.add_anisotropyRotation(m.anisotropyRotation);
    msb.add_anisotropyTexGuid(fbAn);
    msb.add_dispersion(m.dispersion);
    fbMats.push_back(msb.Finish());
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
  std::vector<flatbuffers::Offset<flatbuffers::String>> fbVarNames;
  for (size_t i = 0; i < data.variantNames.size(); ++i) {
    fbVarNames.push_back(builder.CreateString(data.variantNames[i]));
  }
  auto fbVarNamesVec = builder.CreateVector(fbVarNames);
  std::vector<flatbuffers::Offset<KizuriAssets::VariantAssign>> fbVarAssigns;
  for (size_t i = 0; i < data.variantAssigns.size(); ++i) {
    const MeshVariantAssign& a = data.variantAssigns[i];
    KizuriAssets::VariantAssignBuilder vab(builder);
    vab.add_part(a.part);
    vab.add_variant(a.variant);
    vab.add_material(a.material);
    fbVarAssigns.push_back(vab.Finish());
  }
  auto fbVarAssignsVec = builder.CreateVector(fbVarAssigns);
  std::vector<flatbuffers::Offset<KizuriAssets::GltfLight>> fbLights;
  for (size_t i = 0; i < data.lights.size(); ++i) {
    const MeshLightData& l = data.lights[i];
    auto fbLName = builder.CreateString(l.name);
    std::vector<float> fbMat(l.matrix, l.matrix + 16);
    auto fbMatVec = builder.CreateVector(fbMat);
    KizuriAssets::GltfLightBuilder glb(builder);
    glb.add_name(fbLName);
    glb.add_type(l.type);
    glb.add_colorR(l.color[0]);
    glb.add_colorG(l.color[1]);
    glb.add_colorB(l.color[2]);
    glb.add_intensity(l.intensity);
    glb.add_range(l.range);
    glb.add_innerCone(l.innerCone);
    glb.add_outerCone(l.outerCone);
    glb.add_matrix(fbMatVec);
    fbLights.push_back(glb.Finish());
  }
  auto fbLightsVec = builder.CreateVector(fbLights);
  std::vector<flatbuffers::Offset<KizuriAssets::GpuInstance>> fbInsts;
  for (size_t i = 0; i < data.gpuInstances.size(); ++i) {
    const MeshGpuInstance& g = data.gpuInstances[i];
    auto fbIMat = builder.CreateVector(g.matrices);
    KizuriAssets::GpuInstanceBuilder gib(builder);
    gib.add_mesh(g.mesh);
    gib.add_matrices(fbIMat);
    fbInsts.push_back(gib.Finish());
  }
  auto fbInstsVec = builder.CreateVector(fbInsts);
  KizuriAssets::MeshAssetBuilder mab(builder);
  mab.add_guid(fbGuid);
  mab.add_vertexData(fbVerts);
  mab.add_vertexCount(static_cast<uint32_t>(vertexCount));
  mab.add_vertexStride(40);
  mab.add_tangentData(fbTans);
  mab.add_indexData(fbIdx);
  mab.add_indexCount(static_cast<uint32_t>(data.indices.size()));
  mab.add_bounds(&bounds);
  mab.add_materials(fbMatsVec);
  mab.add_parts(fbPartsVec);
  mab.add_hasSource(data.hasSource);
  mab.add_sourcePath(fbSourcePath);
  mab.add_sourceHash(data.sourceHash);
  mab.add_sourceTimestamp(static_cast<int64_t>(data.sourceTimestamp));
  mab.add_variantNames(fbVarNamesVec);
  mab.add_variantAssigns(fbVarAssignsVec);
  mab.add_lights(fbLightsVec);
  mab.add_gpuInstances(fbInstsVec);
  auto mesh = mab.Finish();
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
  if (!data.uv2s.empty() && data.uv2s.size() != vertexCount * 2) {
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
  if (mesh->vertexStride() != 32 && mesh->vertexStride() != 40) {
    return false;
  }
  size_t vstride = mesh->vertexStride() / 4;
  if (mesh->vertexData()->size() != mesh->vertexCount() * mesh->vertexStride() || mesh->indexData()->size() != mesh->indexCount() * 4) {
    return false;
  }
  out.guid = mesh->guid()->str();
  size_t vertexCount = mesh->vertexCount();
  out.positions.resize(vertexCount * 3);
  out.normals.resize(vertexCount * 3);
  out.uvs.resize(vertexCount * 2);
  out.tangents.resize(vertexCount * 4);
  out.uv2s.resize(vertexCount * 2);
  const float* vdata = reinterpret_cast<const float*>(mesh->vertexData()->data());
  for (size_t i = 0; i < vertexCount; ++i) {
    out.positions[i * 3 + 0] = vdata[i * vstride + 0];
    out.positions[i * 3 + 1] = vdata[i * vstride + 1];
    out.positions[i * 3 + 2] = vdata[i * vstride + 2];
    out.normals[i * 3 + 0] = vdata[i * vstride + 3];
    out.normals[i * 3 + 1] = vdata[i * vstride + 4];
    out.normals[i * 3 + 2] = vdata[i * vstride + 5];
    out.uvs[i * 2 + 0] = vdata[i * vstride + 6];
    out.uvs[i * 2 + 1] = vdata[i * vstride + 7];
    if (vstride >= 10) {
      out.uv2s[i * 2 + 0] = vdata[i * vstride + 8];
      out.uv2s[i * 2 + 1] = vdata[i * vstride + 9];
    } else {
      out.uv2s[i * 2 + 0] = 0.0f;
      out.uv2s[i * 2 + 1] = 0.0f;
    }
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
      md.alpha = m->alpha();
      md.metallic = m->metallic();
      md.roughness = m->roughness();
      md.albedoTexGuid = m->albedoTexGuid() != nullptr ? m->albedoTexGuid()->str() : "";
      md.normalTexGuid = m->normalTexGuid() != nullptr ? m->normalTexGuid()->str() : "";
      md.ormTexGuid = m->ormTexGuid() != nullptr ? m->ormTexGuid()->str() : "";
      md.heightTexGuid = m->heightTexGuid() != nullptr ? m->heightTexGuid()->str() : "";
      md.emissive[0] = m->emissiveR();
      md.emissive[1] = m->emissiveG();
      md.emissive[2] = m->emissiveB();
      md.emissiveStrength = m->emissiveStrength();
      md.emissiveTexGuid = m->emissiveTexGuid() != nullptr ? m->emissiveTexGuid()->str() : "";
      md.unlit = m->unlit();
      md.doubleSided = m->doubleSided();
      md.alphaMode = m->alphaMode();
      md.alphaCutoff = m->alphaCutoff();
      if (m->xforms() != nullptr) {
        uint32_t xn = m->xforms()->size() < 5 ? m->xforms()->size() : 5;
        for (uint32_t s = 0; s < xn; ++s) {
          const KizuriAssets::UvXform* x = m->xforms()->Get(s);
          md.xforms[s].offsetU = x->offsetU();
          md.xforms[s].offsetV = x->offsetV();
          md.xforms[s].scaleU = x->scaleU();
          md.xforms[s].scaleV = x->scaleV();
          md.xforms[s].rotation = x->rotation();
          md.xforms[s].uvSet = x->uvSet();
        }
      }
      md.ior = m->ior();
      md.hasSpecular = m->hasSpecular();
      md.specularColor[0] = m->specularR();
      md.specularColor[1] = m->specularG();
      md.specularColor[2] = m->specularB();
      md.specularStrength = m->specularStrength();
      md.specularTexGuid = m->specularTexGuid() != nullptr ? m->specularTexGuid()->str() : "";
      md.specularColorTexGuid = m->specularColorTexGuid() != nullptr ? m->specularColorTexGuid()->str() : "";
      md.clearcoat = m->clearcoat();
      md.clearcoatRoughness = m->clearcoatRoughness();
      md.clearcoatTexGuid = m->clearcoatTexGuid() != nullptr ? m->clearcoatTexGuid()->str() : "";
      md.clearcoatRoughnessTexGuid = m->clearcoatRoughnessTexGuid() != nullptr ? m->clearcoatRoughnessTexGuid()->str() : "";
      md.clearcoatNormalTexGuid = m->clearcoatNormalTexGuid() != nullptr ? m->clearcoatNormalTexGuid()->str() : "";
      md.sheenColor[0] = m->sheenColorR();
      md.sheenColor[1] = m->sheenColorG();
      md.sheenColor[2] = m->sheenColorB();
      md.sheenRoughness = m->sheenRoughness();
      md.sheenColorTexGuid = m->sheenColorTexGuid() != nullptr ? m->sheenColorTexGuid()->str() : "";
      md.sheenRoughnessTexGuid = m->sheenRoughnessTexGuid() != nullptr ? m->sheenRoughnessTexGuid()->str() : "";
      md.transmission = m->transmission();
      md.transmissionTexGuid = m->transmissionTexGuid() != nullptr ? m->transmissionTexGuid()->str() : "";
      md.thickness = m->thickness();
      md.thicknessTexGuid = m->thicknessTexGuid() != nullptr ? m->thicknessTexGuid()->str() : "";
      md.attenuationColor[0] = m->attenuationR();
      md.attenuationColor[1] = m->attenuationG();
      md.attenuationColor[2] = m->attenuationB();
      md.attenuationDistance = m->attenuationDistance();
      md.iridescence = m->iridescence();
      md.iridescenceIor = m->iridescenceIor();
      md.iridescenceThicknessMin = m->iridescenceThicknessMin();
      md.iridescenceThicknessMax = m->iridescenceThicknessMax();
      md.iridescenceTexGuid = m->iridescenceTexGuid() != nullptr ? m->iridescenceTexGuid()->str() : "";
      md.iridescenceThicknessTexGuid = m->iridescenceThicknessTexGuid() != nullptr ? m->iridescenceThicknessTexGuid()->str() : "";
      md.anisotropy = m->anisotropy();
      md.anisotropyRotation = m->anisotropyRotation();
      md.anisotropyTexGuid = m->anisotropyTexGuid() != nullptr ? m->anisotropyTexGuid()->str() : "";
      md.dispersion = m->dispersion();
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
  if (mesh->variantNames() != nullptr) {
    for (uint32_t i = 0; i < mesh->variantNames()->size(); ++i) {
      out.variantNames.push_back(mesh->variantNames()->Get(i) != nullptr ? mesh->variantNames()->Get(i)->str() : "");
    }
  }
  if (mesh->variantAssigns() != nullptr) {
    for (uint32_t i = 0; i < mesh->variantAssigns()->size(); ++i) {
      const KizuriAssets::VariantAssign* a = mesh->variantAssigns()->Get(i);
      MeshVariantAssign va;
      va.part = a->part();
      va.variant = a->variant();
      va.material = a->material();
      out.variantAssigns.push_back(va);
    }
  }
  if (mesh->lights() != nullptr) {
    for (uint32_t i = 0; i < mesh->lights()->size(); ++i) {
      const KizuriAssets::GltfLight* l = mesh->lights()->Get(i);
      MeshLightData ld;
      ld.name = l->name() != nullptr ? l->name()->str() : "";
      ld.type = l->type();
      ld.color[0] = l->colorR();
      ld.color[1] = l->colorG();
      ld.color[2] = l->colorB();
      ld.intensity = l->intensity();
      ld.range = l->range();
      ld.innerCone = l->innerCone();
      ld.outerCone = l->outerCone();
      if (l->matrix() != nullptr) {
        uint32_t mn = l->matrix()->size() < 16 ? l->matrix()->size() : 16;
        for (uint32_t k = 0; k < mn; ++k) {
          ld.matrix[k] = l->matrix()->Get(k);
        }
      }
      out.lights.push_back(ld);
    }
  }
  if (mesh->gpuInstances() != nullptr) {
    for (uint32_t i = 0; i < mesh->gpuInstances()->size(); ++i) {
      const KizuriAssets::GpuInstance* g = mesh->gpuInstances()->Get(i);
      MeshGpuInstance gi;
      gi.mesh = g->mesh();
      if (g->matrices() != nullptr) {
        for (uint32_t k = 0; k < g->matrices()->size(); ++k) {
          gi.matrices.push_back(g->matrices()->Get(k));
        }
      }
      out.gpuInstances.push_back(gi);
    }
  }
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
bool OptimizeVertexCache(MeshAssetData& data) {
  if (data.indices.empty() || data.positions.size() < 3) {
    return false;
  }
  size_t vcount = data.positions.size() / 3;
  std::vector<unsigned int> tmp;
  tmp.reserve(1024);
  bool anyOk = false;
  if (data.parts.empty()) {
    tmp.assign(data.indices.size(), 0);
    meshopt_optimizeVertexCache(tmp.data(), reinterpret_cast<const unsigned int*>(data.indices.data()), data.indices.size(), vcount);
    for (size_t i = 0; i < data.indices.size(); ++i) {
      data.indices[i] = tmp[i];
    }
    return true;
  }
  for (size_t pi = 0; pi < data.parts.size(); ++pi) {
    uint32_t off = data.parts[pi].indexOffset;
    uint32_t count = data.parts[pi].indexCount;
    if (count == 0 || count % 3 != 0) {
      continue;
    }
    if (static_cast<size_t>(off) + static_cast<size_t>(count) > data.indices.size()) {
      continue;
    }
    tmp.assign(count, 0);
    meshopt_optimizeVertexCache(tmp.data(), reinterpret_cast<const unsigned int*>(&data.indices[off]), count, vcount);
    for (uint32_t i = 0; i < count; ++i) {
      data.indices[off + i] = tmp[i];
    }
    anyOk = true;
  }
  return anyOk;
}
bool BuildMergedMesh(const std::vector<StaticMergePart>& parts, MeshAssetData& out) { out = MeshAssetData();
  if (parts.empty()) {
    return false;
  }
  size_t totalVerts = 0;
  size_t totalIdx = 0;
  for (size_t i = 0; i < parts.size(); ++i) {
    const StaticMergePart& p = parts[i];
    if (p.data == nullptr || p.indexCount == 0 || p.indexCount % 3 != 0) {
      return false;
    }
    size_t vc = p.data->positions.size() / 3;
    if (vc == 0 || p.data->normals.size() != vc * 3) {
      return false;
    }
    if (static_cast<size_t>(p.indexOffset) + static_cast<size_t>(p.indexCount) > p.data->indices.size()) {
      return false;
    }
    totalVerts += vc;
    totalIdx += p.indexCount;
    if (totalVerts > 1000000 || totalIdx > 3000000) {
      return false;
    }
  }
  out.positions.reserve(totalVerts * 3);
  out.normals.reserve(totalVerts * 3);
  out.uvs.reserve(totalVerts * 2);
  out.tangents.reserve(totalVerts * 4);
  out.indices.reserve(totalIdx);
  bool needTangentGen = false;
  for (size_t i = 0; i < parts.size(); ++i) {
    const StaticMergePart& p = parts[i];
    const MeshAssetData& d = *p.data;
    size_t vc = d.positions.size() / 3;
    uint32_t base = static_cast<uint32_t>(out.positions.size() / 3);
    const float* m = p.world;
    float a00 = m[0];
    float a01 = m[1];
    float a02 = m[2];
    float a10 = m[4];
    float a11 = m[5];
    float a12 = m[6];
    float a20 = m[8];
    float a21 = m[9];
    float a22 = m[10];
    float det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
    float inv[9];
    if (det > 1e-12f || det < -1e-12f) {
      float id = 1.0f / det;
      inv[0] = (a11 * a22 - a12 * a21) * id;
      inv[1] = (a02 * a21 - a01 * a22) * id;
      inv[2] = (a01 * a12 - a02 * a11) * id;
      inv[3] = (a12 * a20 - a10 * a22) * id;
      inv[4] = (a00 * a22 - a02 * a20) * id;
      inv[5] = (a02 * a10 - a00 * a12) * id;
      inv[6] = (a10 * a21 - a11 * a20) * id;
      inv[7] = (a01 * a20 - a00 * a21) * id;
      inv[8] = (a00 * a11 - a01 * a10) * id;
    } else {
      inv[0] = 1.0f;
      inv[1] = 0.0f;
      inv[2] = 0.0f;
      inv[3] = 0.0f;
      inv[4] = 1.0f;
      inv[5] = 0.0f;
      inv[6] = 0.0f;
      inv[7] = 0.0f;
      inv[8] = 1.0f;
    }
    bool hasUv = d.uvs.size() == vc * 2;
    bool hasTan = d.tangents.size() == vc * 4;
    if (!hasTan) {
      needTangentGen = true;
    }
    for (size_t v = 0; v < vc; ++v) {
      float x = d.positions[v * 3 + 0];
      float y = d.positions[v * 3 + 1];
      float z = d.positions[v * 3 + 2];
      out.positions.push_back(x * m[0] + y * m[4] + z * m[8] + m[12]);
      out.positions.push_back(x * m[1] + y * m[5] + z * m[9] + m[13]);
      out.positions.push_back(x * m[2] + y * m[6] + z * m[10] + m[14]);
      float nx = d.normals[v * 3 + 0];
      float ny = d.normals[v * 3 + 1];
      float nz = d.normals[v * 3 + 2];
      float wx = nx * inv[0] + ny * inv[3] + nz * inv[6];
      float wy = nx * inv[1] + ny * inv[4] + nz * inv[7];
      float wz = nx * inv[2] + ny * inv[5] + nz * inv[8];
      float wl = std::sqrt(wx * wx + wy * wy + wz * wz);
      if (wl < 1e-12f) {
        wx = 0.0f;
        wy = 1.0f;
        wz = 0.0f;
        wl = 1.0f;
      }
      out.normals.push_back(wx / wl);
      out.normals.push_back(wy / wl);
      out.normals.push_back(wz / wl);
      out.uvs.push_back(hasUv ? d.uvs[v * 2 + 0] : 0.0f);
      out.uvs.push_back(hasUv ? d.uvs[v * 2 + 1] : 0.0f);
      if (hasTan) {
        out.tangents.push_back(d.tangents[v * 4 + 0]);
        out.tangents.push_back(d.tangents[v * 4 + 1]);
        out.tangents.push_back(d.tangents[v * 4 + 2]);
        out.tangents.push_back(d.tangents[v * 4 + 3]);
      } else {
        out.tangents.push_back(1.0f);
        out.tangents.push_back(0.0f);
        out.tangents.push_back(0.0f);
        out.tangents.push_back(1.0f);
      }
    }
    for (uint32_t k = 0; k < p.indexCount; ++k) {
      out.indices.push_back(base + d.indices[static_cast<size_t>(p.indexOffset) + k]);
    }
  }
  if (needTangentGen) {
    std::vector<float> gen;
    if (GenerateTangents(out.positions, out.normals, out.uvs, out.indices, gen) && gen.size() == out.positions.size() / 3 * 4) {
      out.tangents = gen;
    }
  }
  MeshPartData part;
  part.indexOffset = 0;
  part.indexCount = static_cast<uint32_t>(out.indices.size());
  part.material = 0;
  out.parts.push_back(part);
  ComputeAABB(out.positions, out.aabbMin, out.aabbMax);
  return !out.indices.empty();
}
bool BuildMeshlets(MeshAssetData& data, std::vector<MeshletRange>& outRanges) {
  outRanges.clear();
  size_t vcount = data.positions.size() / 3;
  if (vcount == 0 || data.indices.size() < 768 || data.indices.size() % 3 != 0) {
    return false;
  }
  if (data.normals.size() != vcount * 3) {
    return false;
  }
  const size_t maxVerts = 64;
  const size_t maxTris = 124;
  struct PartWork {
    uint32_t srcOff;
    uint32_t srcCount;
    uint32_t mat;
  };
  std::vector<PartWork> work;
  if (data.parts.empty()) {
    PartWork w;
    w.srcOff = 0;
    w.srcCount = static_cast<uint32_t>(data.indices.size());
    w.mat = 0;
    work.push_back(w);
  } else {
    for (size_t pi = 0; pi < data.parts.size(); ++pi) {
      const MeshPartData& pd = data.parts[pi];
      if (pd.indexCount == 0 || pd.indexCount % 3 != 0) {
        return false;
      }
      if (static_cast<size_t>(pd.indexOffset) + static_cast<size_t>(pd.indexCount) > data.indices.size()) {
        return false;
      }
      PartWork w;
      w.srcOff = pd.indexOffset;
      w.srcCount = pd.indexCount;
      w.mat = pd.material;
      work.push_back(w);
    }
  }
  bool hasUv = data.uvs.size() == vcount * 2;
  bool hasTan = data.tangents.size() == vcount * 4;
  bool hasUv2 = data.uv2s.size() == vcount * 2;
  std::vector<float> npos;
  std::vector<float> nnrm;
  std::vector<float> nuv;
  std::vector<float> ntan;
  std::vector<float> nuv2;
  std::vector<uint32_t> nidx;
  std::vector<MeshPartData> nparts;
  nidx.reserve(data.indices.size());
  for (size_t wi = 0; wi < work.size(); ++wi) {
    const PartWork& w = work[wi];
    std::vector<unsigned int> idx(w.srcCount);
    for (uint32_t i = 0; i < w.srcCount; ++i) {
      idx[i] = data.indices[static_cast<size_t>(w.srcOff) + i];
    }
    struct LocalMeshlet {
      std::vector<unsigned int> verts;
      std::vector<unsigned int> tris;
    };
    std::vector<LocalMeshlet> localMeshlets;
    {
      size_t triCount = idx.size() / 3;
      std::vector<std::vector<unsigned int>> vertTris(vcount);
      for (size_t t = 0; t < triCount; ++t) {
        for (int c = 0; c < 3; ++c) {
          unsigned int v = idx[t * 3 + c];
          if (v < vcount) {
            vertTris[v].push_back(static_cast<unsigned int>(t));
          }
        }
      }
      std::vector<unsigned char> triDone(triCount, 0);
      std::vector<unsigned int> vertMark(vcount, 0);
      unsigned int curMark = 1;
      for (size_t seed = 0; seed < triCount; ++seed) {
        if (triDone[seed]) {
          continue;
        }
        LocalMeshlet ml;
        std::vector<unsigned int> stack;
        stack.push_back(static_cast<unsigned int>(seed));
        while (!stack.empty() && ml.tris.size() < maxTris) {
          unsigned int t = stack.back();
          stack.pop_back();
          if (triDone[t]) {
            continue;
          }
          unsigned int v0 = idx[t * 3 + 0];
          unsigned int v1 = idx[t * 3 + 1];
          unsigned int v2 = idx[t * 3 + 2];
          unsigned int add = 0;
          if (v0 < vcount && vertMark[v0] != curMark) {
            ++add;
          }
          if (v1 < vcount && vertMark[v1] != curMark) {
            ++add;
          }
          if (v2 < vcount && vertMark[v2] != curMark) {
            ++add;
          }
          if (ml.verts.size() + add > maxVerts) {
            continue;
          }
          triDone[t] = 1;
          ml.tris.push_back(t);
          if (v0 < vcount && vertMark[v0] != curMark) {
            vertMark[v0] = curMark;
            ml.verts.push_back(v0);
          }
          if (v1 < vcount && vertMark[v1] != curMark) {
            vertMark[v1] = curMark;
            ml.verts.push_back(v1);
          }
          if (v2 < vcount && vertMark[v2] != curMark) {
            vertMark[v2] = curMark;
            ml.verts.push_back(v2);
          }
          unsigned int vs[3] = { v0, v1, v2 };
          for (int c = 0; c < 3; ++c) {
            if (vs[c] >= vcount) {
              continue;
            }
            const std::vector<unsigned int>& adj = vertTris[vs[c]];
            for (size_t a = 0; a < adj.size(); ++a) {
              if (!triDone[adj[a]]) {
                stack.push_back(adj[a]);
              }
            }
          }
        }
        ++curMark;
        if (curMark == 0) {
          std::fill(vertMark.begin(), vertMark.end(), 0);
          curMark = 1;
        }
        if (!ml.tris.empty()) {
          localMeshlets.push_back(ml);
        }
      }
    }
    size_t mcount = localMeshlets.size();
    uint32_t partBase = static_cast<uint32_t>(nidx.size());
    if (mcount < 2) {
      float bmin[3] = { 1e30f, 1e30f, 1e30f };
      float bmax[3] = { -1e30f, -1e30f, -1e30f };
      for (uint32_t i = 0; i < w.srcCount; ++i) {
        unsigned int v = idx[i];
        if (v >= vcount) {
          return false;
        }
        for (int c = 0; c < 3; ++c) {
          float vv = data.positions[v * 3 + c];
          if (vv < bmin[c]) {
            bmin[c] = vv;
          }
          if (vv > bmax[c]) {
            bmax[c] = vv;
          }
        }
      }
      float cc[3] = { (bmin[0] + bmax[0]) * 0.5f, (bmin[1] + bmax[1]) * 0.5f, (bmin[2] + bmax[2]) * 0.5f };
      float cr = 0.0f;
      for (uint32_t i = 0; i < w.srcCount; ++i) {
        unsigned int v = idx[i];
        float dx = data.positions[v * 3 + 0] - cc[0];
        float dy = data.positions[v * 3 + 1] - cc[1];
        float dz = data.positions[v * 3 + 2] - cc[2];
        float dd = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dd > cr) {
          cr = dd;
        }
      }
      uint32_t vbase0 = static_cast<uint32_t>(npos.size() / 3);
      for (size_t v = 0; v < vcount; ++v) {
        npos.push_back(data.positions[v * 3 + 0]);
        npos.push_back(data.positions[v * 3 + 1]);
        npos.push_back(data.positions[v * 3 + 2]);
        nnrm.push_back(data.normals[v * 3 + 0]);
        nnrm.push_back(data.normals[v * 3 + 1]);
        nnrm.push_back(data.normals[v * 3 + 2]);
        nuv.push_back(hasUv ? data.uvs[v * 2 + 0] : 0.0f);
        nuv.push_back(hasUv ? data.uvs[v * 2 + 1] : 0.0f);
        if (hasTan) {
          ntan.push_back(data.tangents[v * 4 + 0]);
          ntan.push_back(data.tangents[v * 4 + 1]);
          ntan.push_back(data.tangents[v * 4 + 2]);
          ntan.push_back(data.tangents[v * 4 + 3]);
        } else {
          ntan.push_back(1.0f);
          ntan.push_back(0.0f);
          ntan.push_back(0.0f);
          ntan.push_back(1.0f);
        }
        if (hasUv2) {
          nuv2.push_back(data.uv2s[v * 2 + 0]);
          nuv2.push_back(data.uv2s[v * 2 + 1]);
        }
      }
      for (uint32_t i = 0; i < w.srcCount; ++i) {
        nidx.push_back(vbase0 + data.indices[static_cast<size_t>(w.srcOff) + i]);
      }
      MeshletRange range;
      range.indexOffset = partBase;
      range.indexCount = w.srcCount;
      range.apex[0] = cc[0];
      range.apex[1] = cc[1];
      range.apex[2] = cc[2];
      range.axis[0] = 0.0f;
      range.axis[1] = 0.0f;
      range.axis[2] = 0.0f;
      range.cutoff = 1.0f;
      range.center[0] = cc[0];
      range.center[1] = cc[1];
      range.center[2] = cc[2];
      range.radius = cr;
      outRanges.push_back(range);
    } else {
      for (size_t m = 0; m < mcount; ++m) {
        LocalMeshlet& ml = localMeshlets[m];
        float bmin[3] = { 1e30f, 1e30f, 1e30f };
        float bmax[3] = { -1e30f, -1e30f, -1e30f };
        for (size_t vi = 0; vi < ml.verts.size(); ++vi) {
          unsigned int src = ml.verts[vi];
          for (int c = 0; c < 3; ++c) {
            float v = data.positions[src * 3 + c];
            if (v < bmin[c]) {
              bmin[c] = v;
            }
            if (v > bmax[c]) {
              bmax[c] = v;
            }
          }
        }
        float cc[3] = { (bmin[0] + bmax[0]) * 0.5f, (bmin[1] + bmax[1]) * 0.5f, (bmin[2] + bmax[2]) * 0.5f };
        float cr = 0.0f;
        for (size_t vi = 0; vi < ml.verts.size(); ++vi) {
          unsigned int src = ml.verts[vi];
          float dx = data.positions[src * 3 + 0] - cc[0];
          float dy = data.positions[src * 3 + 1] - cc[1];
          float dz = data.positions[src * 3 + 2] - cc[2];
          float dd = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (dd > cr) {
            cr = dd;
          }
        }
        uint32_t vbase = static_cast<uint32_t>(npos.size() / 3);
        for (size_t vi = 0; vi < ml.verts.size(); ++vi) {
          unsigned int src = ml.verts[vi];
          if (src >= vcount) {
            return false;
          }
          npos.push_back(data.positions[src * 3 + 0]);
          npos.push_back(data.positions[src * 3 + 1]);
          npos.push_back(data.positions[src * 3 + 2]);
          nnrm.push_back(data.normals[src * 3 + 0]);
          nnrm.push_back(data.normals[src * 3 + 1]);
          nnrm.push_back(data.normals[src * 3 + 2]);
          nuv.push_back(hasUv ? data.uvs[src * 2 + 0] : 0.0f);
          nuv.push_back(hasUv ? data.uvs[src * 2 + 1] : 0.0f);
          if (hasTan) {
            ntan.push_back(data.tangents[src * 4 + 0]);
            ntan.push_back(data.tangents[src * 4 + 1]);
            ntan.push_back(data.tangents[src * 4 + 2]);
            ntan.push_back(data.tangents[src * 4 + 3]);
          } else {
            ntan.push_back(1.0f);
            ntan.push_back(0.0f);
            ntan.push_back(0.0f);
            ntan.push_back(1.0f);
          }
          if (hasUv2) {
            nuv2.push_back(data.uv2s[src * 2 + 0]);
            nuv2.push_back(data.uv2s[src * 2 + 1]);
          }
        }
        uint32_t ibase = static_cast<uint32_t>(nidx.size());
        for (size_t ti = 0; ti < ml.tris.size(); ++ti) {
          unsigned int t = ml.tris[ti];
          for (int c = 0; c < 3; ++c) {
            unsigned int gv = idx[t * 3 + c];
            unsigned int local = 0;
            for (size_t vi = 0; vi < ml.verts.size(); ++vi) {
              if (ml.verts[vi] == gv) {
                local = static_cast<unsigned int>(vi);
                break;
              }
            }
            nidx.push_back(vbase + local);
          }
        }
        MeshletRange range;
        range.indexOffset = ibase;
        range.indexCount = static_cast<uint32_t>(ml.tris.size() * 3);
        range.apex[0] = cc[0];
        range.apex[1] = cc[1];
        range.apex[2] = cc[2];
        range.axis[0] = 0.0f;
        range.axis[1] = 0.0f;
        range.axis[2] = 0.0f;
        range.cutoff = 1.0f;
        range.center[0] = cc[0];
        range.center[1] = cc[1];
        range.center[2] = cc[2];
        range.radius = cr;
        outRanges.push_back(range);
      }
    }
    MeshPartData part;
    part.indexOffset = partBase;
    part.indexCount = static_cast<uint32_t>(nidx.size()) - partBase;
    part.material = w.mat;
    nparts.push_back(part);
  }
  if (npos.empty()) {
    return false;
  }
  data.positions = npos;
  data.normals = nnrm;
  data.uvs = nuv;
  data.tangents = ntan;
  data.uv2s = nuv2;
  data.indices = nidx;
  data.parts = nparts;
  ComputeAABB(data.positions, data.aabbMin, data.aabbMax);
  return true;
}
bool MeshletRangeVisible(const float world[16], const MeshletRange& range, const float planes[6][4], const float camPos[3]) {
  if (world == nullptr || planes == nullptr || camPos == nullptr) {
    return true;
  }
  float sx = std::sqrt(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]);
  float sy = std::sqrt(world[4] * world[4] + world[5] * world[5] + world[6] * world[6]);
  float sz = std::sqrt(world[8] * world[8] + world[9] * world[9] + world[10] * world[10]);
  float smax = sx > sy ? sx : sy;
  smax = smax > sz ? smax : sz;
  if (smax < 1e-9f) {
    smax = 1.0f;
  }
  float cx = range.center[0] * world[0] + range.center[1] * world[4] + range.center[2] * world[8] + world[12];
  float cy = range.center[0] * world[1] + range.center[1] * world[5] + range.center[2] * world[9] + world[13];
  float cz = range.center[0] * world[2] + range.center[1] * world[6] + range.center[2] * world[10] + world[14];
  float cr = range.radius * smax;
  for (int p = 0; p < 6; ++p) {
    if (planes[p][0] * cx + planes[p][1] * cy + planes[p][2] * cz + planes[p][3] < -cr) {
      return false;
    }
  }
  float ax = range.axis[0] * world[0] + range.axis[1] * world[4] + range.axis[2] * world[8];
  float ay = range.axis[0] * world[1] + range.axis[1] * world[5] + range.axis[2] * world[9];
  float az = range.axis[0] * world[2] + range.axis[1] * world[6] + range.axis[2] * world[10];
  float alen = std::sqrt(ax * ax + ay * ay + az * az);
  if (alen < 1e-9f || range.cutoff >= 0.9999f) {
    return true;
  }
  ax /= alen;
  ay /= alen;
  az /= alen;
  float px = range.apex[0] * world[0] + range.apex[1] * world[4] + range.apex[2] * world[8] + world[12];
  float py = range.apex[0] * world[1] + range.apex[1] * world[5] + range.apex[2] * world[9] + world[13];
  float pz = range.apex[0] * world[2] + range.apex[1] * world[6] + range.apex[2] * world[10] + world[14];
  float vx = camPos[0] - px;
  float vy = camPos[1] - py;
  float vz = camPos[2] - pz;
  float vlen = std::sqrt(vx * vx + vy * vy + vz * vz);
  if (vlen < 1e-9f) {
    return true;
  }
  float d = (vx * ax + vy * ay + vz * az) / vlen;
  return d < range.cutoff;
}
}
