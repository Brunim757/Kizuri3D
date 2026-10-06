#pragma once
#include <string>
#include <stdint.h>
#include <vector>
namespace Kizuri {
struct MeshUvXform {
  float offsetU = 0.0f;
  float offsetV = 0.0f;
  float scaleU = 1.0f;
  float scaleV = 1.0f;
  float rotation = 0.0f;
  int uvSet = 0;
};
struct MeshMaterialData {
  std::string name;
  float albedo[3] = { 1.0f, 1.0f, 1.0f };
  float alpha = 1.0f;
  float metallic = 0.0f;
  float roughness = 1.0f;
  std::string albedoTexGuid;
  std::string normalTexGuid;
  std::string ormTexGuid;
  std::string heightTexGuid;
  float emissive[3] = { 0.0f, 0.0f, 0.0f };
  float emissiveStrength = 1.0f;
  std::string emissiveTexGuid;
  bool unlit = false;
  bool doubleSided = false;
  int alphaMode = 0;
  float alphaCutoff = 0.5f;
  MeshUvXform xforms[5];
  float ior = 1.5f;
  bool hasSpecular = false;
  float specularColor[3] = { 1.0f, 1.0f, 1.0f };
  float specularStrength = 1.0f;
  std::string specularTexGuid;
  std::string specularColorTexGuid;
  float clearcoat = 0.0f;
  float clearcoatRoughness = 0.0f;
  std::string clearcoatTexGuid;
  std::string clearcoatRoughnessTexGuid;
  std::string clearcoatNormalTexGuid;
  float sheenColor[3] = { 0.0f, 0.0f, 0.0f };
  float sheenRoughness = 0.0f;
  std::string sheenColorTexGuid;
  std::string sheenRoughnessTexGuid;
  float transmission = 0.0f;
  std::string transmissionTexGuid;
  float thickness = 0.0f;
  std::string thicknessTexGuid;
  float attenuationColor[3] = { 1.0f, 1.0f, 1.0f };
  float attenuationDistance = 0.0f;
  float iridescence = 0.0f;
  float iridescenceIor = 1.3f;
  float iridescenceThicknessMin = 100.0f;
  float iridescenceThicknessMax = 400.0f;
  std::string iridescenceTexGuid;
  std::string iridescenceThicknessTexGuid;
  float anisotropy = 0.0f;
  float anisotropyRotation = 0.0f;
  std::string anisotropyTexGuid;
  float dispersion = 0.0f;
};
struct MeshPartData {
  uint32_t indexOffset;
  uint32_t indexCount;
  uint32_t material;
};
struct MeshVariantAssign {
  uint32_t part;
  uint32_t variant;
  uint32_t material;
};
struct MeshLightData {
  std::string name;
  int type = 0;
  float color[3] = { 1.0f, 1.0f, 1.0f };
  float intensity = 1.0f;
  float range = 0.0f;
  float innerCone = 0.0f;
  float outerCone = 0.7853982f;
  float matrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
};
struct MeshGpuInstance {
  uint32_t mesh;
  std::vector<float> matrices;
};
struct MeshAssetData {
  std::string guid;
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<float> tangents;
  std::vector<float> uv2s;
  std::vector<uint32_t> indices;
  std::vector<MeshMaterialData> materials;
  std::vector<MeshPartData> parts;
  float aabbMin[3];
  float aabbMax[3];
  bool hasSource;
  std::string sourcePath;
  uint64_t sourceHash;
  int64_t sourceTimestamp;
  std::vector<std::string> variantNames;
  std::vector<MeshVariantAssign> variantAssigns;
  std::vector<MeshLightData> lights;
  std::vector<MeshGpuInstance> gpuInstances;
};
bool EncodeMeshFile(const MeshAssetData& data, const std::string& path);
bool DecodeMeshFile(const std::string& path, MeshAssetData& out);
bool EncodeMeshMemory(const MeshAssetData& data, std::vector<unsigned char>& out);
bool DecodeMeshMemory(const void* bytes, size_t size, MeshAssetData& out);
void ComputeAABB(const std::vector<float>& positions, float outMin[3], float outMax[3]);
bool GenerateTangents(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& uvs, const std::vector<uint32_t>& indices, std::vector<float>& tangentsOut);
void OrthogonalizeTangents(const std::vector<float>& normals, std::vector<float>& tangents);
bool GenerateLodMesh(const MeshAssetData& src, float targetRatio, float targetError, MeshAssetData& out);
bool UnwrapPrim(const std::vector<float>& inPos, const std::vector<float>& inNrm, const std::vector<float>& inUv, const std::vector<float>& inTan, const std::vector<uint32_t>& inIdx, std::vector<float>& outPos, std::vector<float>& outNrm, std::vector<float>& outUv, std::vector<float>& outTan, std::vector<uint32_t>& outIdx, std::vector<float>& outUv2);
}