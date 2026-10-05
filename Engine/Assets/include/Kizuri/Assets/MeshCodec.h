#pragma once
#include <string>
#include <stdint.h>
#include <vector>
namespace Kizuri {
struct MeshMaterialData {
  std::string name;
  float albedo[3];
  float metallic;
  float roughness;
  std::string albedoTexGuid;
  std::string normalTexGuid;
  std::string ormTexGuid;
  std::string heightTexGuid;
};
struct MeshPartData {
  uint32_t indexOffset;
  uint32_t indexCount;
  uint32_t material;
};
struct MeshAssetData {
  std::string guid;
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<float> tangents;
  std::vector<uint32_t> indices;
  std::vector<MeshMaterialData> materials;
  std::vector<MeshPartData> parts;
  float aabbMin[3];
  float aabbMax[3];
  bool hasSource;
  std::string sourcePath;
  uint64_t sourceHash;
  int64_t sourceTimestamp;
};
bool EncodeMeshFile(const MeshAssetData& data, const std::string& path);
bool DecodeMeshFile(const std::string& path, MeshAssetData& out);
bool EncodeMeshMemory(const MeshAssetData& data, std::vector<unsigned char>& out);
bool DecodeMeshMemory(const void* bytes, size_t size, MeshAssetData& out);
void ComputeAABB(const std::vector<float>& positions, float outMin[3], float outMax[3]);
bool GenerateTangents(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& uvs, const std::vector<uint32_t>& indices, std::vector<float>& tangentsOut);
void OrthogonalizeTangents(const std::vector<float>& normals, std::vector<float>& tangents);
bool GenerateLodMesh(const MeshAssetData& src, float targetRatio, float targetError, MeshAssetData& out);
}