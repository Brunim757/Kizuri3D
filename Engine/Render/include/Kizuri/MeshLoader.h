#pragma once
#include <vector>
#include <stdint.h>
namespace Kizuri {
struct StaticMesh {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<float> tangents;
  std::vector<float> uv2s;
  std::vector<uint32_t> indices;
};
bool LoadStaticMeshFromGltf(const char* path, StaticMesh& out);
}
