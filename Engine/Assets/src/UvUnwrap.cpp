#include "Kizuri/Assets/MeshCodec.h"
#include <xatlas.h>
namespace Kizuri {
bool UnwrapPrim(const std::vector<float>& inPos, const std::vector<float>& inNrm, const std::vector<float>& inUv, const std::vector<float>& inTan, const std::vector<uint32_t>& inIdx, std::vector<float>& outPos, std::vector<float>& outNrm, std::vector<float>& outUv, std::vector<float>& outTan, std::vector<uint32_t>& outIdx, std::vector<float>& outUv2) {
  outPos.clear();
  outNrm.clear();
  outUv.clear();
  outTan.clear();
  outIdx.clear();
  outUv2.clear();
  size_t vertCount = inPos.size() / 3;
  if (vertCount == 0 || vertCount > 4000000000u) {
    return false;
  }
  if (inNrm.size() != inPos.size() || inUv.size() != vertCount * 2 || inTan.size() != vertCount * 4) {
    return false;
  }
  if (inIdx.empty() || inIdx.size() % 3 != 0) {
    return false;
  }
  for (size_t i = 0; i < inIdx.size(); ++i) {
    if (inIdx[i] >= vertCount) {
      return false;
    }
  }
  xatlas::Atlas* atlas = xatlas::Create();
  if (atlas == nullptr) {
    return false;
  }
  xatlas::MeshDecl decl;
  decl.vertexCount = static_cast<uint32_t>(vertCount);
  decl.vertexPositionData = inPos.data();
  decl.vertexPositionStride = sizeof(float) * 3;
  decl.vertexNormalData = inNrm.data();
  decl.vertexNormalStride = sizeof(float) * 3;
  decl.vertexUvData = inUv.data();
  decl.vertexUvStride = sizeof(float) * 2;
  decl.indexData = inIdx.data();
  decl.indexCount = static_cast<uint32_t>(inIdx.size());
  decl.indexFormat = xatlas::IndexFormat::UInt32;
  bool ok = xatlas::AddMesh(atlas, decl) == xatlas::AddMeshError::Success;
  if (ok) {
    xatlas::Generate(atlas);
    ok = atlas->meshCount == 1 && atlas->width > 0 && atlas->height > 0;
  }
  if (ok) {
    const xatlas::Mesh& om = atlas->meshes[0];
    if (om.vertexCount == 0 || om.indexCount == 0 || om.indexCount % 3 != 0) {
      ok = false;
    } else {
      float w = static_cast<float>(atlas->width);
      float h = static_cast<float>(atlas->height);
      outPos.reserve(om.vertexCount * 3);
      outNrm.reserve(om.vertexCount * 3);
      outUv.reserve(om.vertexCount * 2);
      outTan.reserve(om.vertexCount * 4);
      outUv2.reserve(om.vertexCount * 2);
      for (uint32_t i = 0; i < om.vertexCount && ok; ++i) {
        uint32_t xr = om.vertexArray[i].xref;
        if (xr >= vertCount) {
          ok = false;
          break;
        }
        outPos.push_back(inPos[xr * 3 + 0]);
        outPos.push_back(inPos[xr * 3 + 1]);
        outPos.push_back(inPos[xr * 3 + 2]);
        outNrm.push_back(inNrm[xr * 3 + 0]);
        outNrm.push_back(inNrm[xr * 3 + 1]);
        outNrm.push_back(inNrm[xr * 3 + 2]);
        outUv.push_back(inUv[xr * 2 + 0]);
        outUv.push_back(inUv[xr * 2 + 1]);
        outTan.push_back(inTan[xr * 4 + 0]);
        outTan.push_back(inTan[xr * 4 + 1]);
        outTan.push_back(inTan[xr * 4 + 2]);
        outTan.push_back(inTan[xr * 4 + 3]);
        outUv2.push_back(om.vertexArray[i].uv[0] / w);
        outUv2.push_back(om.vertexArray[i].uv[1] / h);
      }
      for (uint32_t i = 0; i < om.indexCount && ok; ++i) {
        if (om.indexArray[i] >= om.vertexCount) {
          ok = false;
          break;
        }
        outIdx.push_back(om.indexArray[i]);
      }
    }
  }
  xatlas::Destroy(atlas);
  if (!ok) {
    outPos.clear();
    outNrm.clear();
    outUv.clear();
    outTan.clear();
    outIdx.clear();
    outUv2.clear();
    return false;
  }
  return true;
}
}
