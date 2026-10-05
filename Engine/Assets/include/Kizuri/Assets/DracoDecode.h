#pragma once
#include <stdint.h>
#include <vector>
namespace Kizuri {
struct DracoMeshData {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<uint32_t> indices;
};
bool DecodeDracoMesh(const void* bytes, size_t size, int posAttId, int nrmAttId, int uvAttId, DracoMeshData& out);
}
