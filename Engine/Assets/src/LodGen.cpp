#include "Kizuri/Assets/MeshCodec.h"
#include "Kizuri/Assets/Guid.h"
#include <meshoptimizer.h>
namespace Kizuri {
namespace {
bool SimplifyRange(const uint32_t* indices, size_t count, const float* positions, size_t vertexCount, size_t targetCount, float targetError, std::vector<uint32_t>& simplified) {
  simplified.assign(indices, indices + count);
  if (count < 6 || targetCount >= count) {
    return true;
  }
  targetCount -= targetCount % 3;
  if (targetCount < 3) {
    targetCount = 3;
  }
  std::vector<uint32_t> tmp(count);
  size_t resultCount = meshopt_simplify(tmp.data(), indices, count, positions, vertexCount, sizeof(float) * 3, targetCount, targetError, 0, nullptr);
  if (resultCount < 3 || resultCount % 3 != 0) {
    return false;
  }
  simplified.assign(tmp.data(), tmp.data() + resultCount);
  return true;
}
}
bool GenerateLodMesh(const MeshAssetData& src, float targetRatio, float targetError, MeshAssetData& out) {
  out = MeshAssetData();
  size_t vertexCount = src.positions.size() / 3;
  if (vertexCount == 0 || src.indices.empty() || src.indices.size() % 3 != 0) {
    return false;
  }
  if (src.normals.size() != src.positions.size() || src.uvs.size() != vertexCount * 2) {
    return false;
  }
  if (targetRatio < 0.05f) {
    targetRatio = 0.05f;
  }
  if (targetRatio > 0.95f) {
    targetRatio = 0.95f;
  }
  if (targetError < 0.0f) {
    targetError = 0.0f;
  }
  out.guid = GenerateGuidString();
  out.positions = src.positions;
  out.normals = src.normals;
  out.uvs = src.uvs;
  out.tangents = src.tangents;
  out.materials = src.materials;
  out.indices.clear();
  out.parts.clear();
  if (src.parts.empty()) {
    size_t targetCount = static_cast<size_t>(static_cast<float>(src.indices.size()) * targetRatio);
    std::vector<uint32_t> simplified;
    if (!SimplifyRange(src.indices.data(), src.indices.size(), src.positions.data(), vertexCount, targetCount, targetError, simplified)) {
      out = MeshAssetData();
      return false;
    }
    out.indices = simplified;
  } else {
    for (size_t pi = 0; pi < src.parts.size(); ++pi) {
      const MeshPartData& part = src.parts[pi];
      if (part.indexOffset + part.indexCount > src.indices.size() || part.indexCount % 3 != 0) {
        out = MeshAssetData();
        return false;
      }
      size_t targetCount = static_cast<size_t>(static_cast<float>(part.indexCount) * targetRatio);
      std::vector<uint32_t> simplified;
      if (!SimplifyRange(src.indices.data() + part.indexOffset, part.indexCount, src.positions.data(), vertexCount, targetCount, targetError, simplified)) {
        out = MeshAssetData();
        return false;
      }
      MeshPartData np;
      np.indexOffset = static_cast<uint32_t>(out.indices.size());
      np.indexCount = static_cast<uint32_t>(simplified.size());
      np.material = part.material;
      out.indices.insert(out.indices.end(), simplified.begin(), simplified.end());
      out.parts.push_back(np);
    }
  }
  if (out.indices.empty()) {
    out = MeshAssetData();
    return false;
  }
  out.aabbMin[0] = src.aabbMin[0];
  out.aabbMin[1] = src.aabbMin[1];
  out.aabbMin[2] = src.aabbMin[2];
  out.aabbMax[0] = src.aabbMax[0];
  out.aabbMax[1] = src.aabbMax[1];
  out.aabbMax[2] = src.aabbMax[2];
  out.hasSource = false;
  out.sourcePath.clear();
  out.sourceHash = 0;
  out.sourceTimestamp = 0;
  return true;
}
std::string LodSiblingPath(const std::string& baseMeshPath, int tier) {
  if (tier < 1 || tier > 2 || baseMeshPath.empty()) {
    return "";
  }
  size_t dot = baseMeshPath.find_last_of('.');
  size_t sep = baseMeshPath.find_last_of("/\\");
  if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) {
    return baseMeshPath + "_lod" + static_cast<char>('0' + tier);
  }
  std::string tag = "_lod" + std::string(1, static_cast<char>('0' + tier));
  return baseMeshPath.substr(0, dot) + tag + baseMeshPath.substr(dot);
}
bool IsAutoLodPath(const std::string& path) {
  if (path.size() < 12) {
    return false;
  }
  std::string tail = path.substr(path.size() - 12);
  for (size_t i = 0; i < tail.size(); ++i) {
    char c = tail[i];
    if (c >= 'A' && c <= 'Z') {
      tail[i] = static_cast<char>(c + ('a' - 'A'));
    }
  }
  return tail == "_lod1.kzmesh" || tail == "_lod2.kzmesh";
}
void SelectAutoLod(float dist, float nearDist, float farDist, int& tierA, int& tierB, float& fadeB) {  tierA = 0;
  tierB = -1;
  fadeB = 0.0f;
  if (farDist <= nearDist) {
    return;
  }
  if (dist <= nearDist) {
    return;
  }
  float span = farDist - nearDist;
  float b1 = nearDist + span / 3.0f;
  float b2 = nearDist + span * 2.0f / 3.0f;
  float hw = span * 0.1f;
  if (dist < b1 - hw) {
    tierA = 0;
    return;
  }
  if (dist < b1 + hw) {
    tierA = 0;
    tierB = 1;
    fadeB = (dist - (b1 - hw)) / (hw * 2.0f);
    return;
  }
  if (dist < b2 - hw) {
    tierA = 1;
    return;
  }
  if (dist < b2 + hw) {
    tierA = 1;
    tierB = 2;
    fadeB = (dist - (b2 - hw)) / (hw * 2.0f);
    return;
  }
  tierA = 2;
}
}
