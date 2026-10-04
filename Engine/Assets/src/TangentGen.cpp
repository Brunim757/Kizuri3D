#include "Kizuri/Assets/MeshCodec.h"
#include <mikktspace.h>
#include <cmath>
namespace Kizuri {
namespace {
struct MikktMesh {
  const float* positions;
  const float* normals;
  const float* uvs;
  const uint32_t* indices;
  size_t vertexCount;
  size_t faceCount;
  float* tanAccum;
  float* signAccum;
};
int MikktGetNumFaces(const SMikkTSpaceContext* ctx) {
  const MikktMesh* m = static_cast<const MikktMesh*>(ctx->m_pUserData);
  return static_cast<int>(m->faceCount);
}
int MikktGetNumVerts(const SMikkTSpaceContext* ctx, int face) {
  (void)ctx;
  (void)face;
  return 3;
}
uint32_t MikktVertIndex(const MikktMesh* m, int face, int vert) {
  size_t idx = static_cast<size_t>(face) * 3 + static_cast<size_t>(vert);
  uint32_t v = m->indices[idx];
  if (v >= m->vertexCount) {
    return 0;
  }
  return v;
}
void MikktGetPosition(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
  const MikktMesh* m = static_cast<const MikktMesh*>(ctx->m_pUserData);
  uint32_t v = MikktVertIndex(m, face, vert);
  out[0] = m->positions[v * 3 + 0];
  out[1] = m->positions[v * 3 + 1];
  out[2] = m->positions[v * 3 + 2];
}
void MikktGetNormal(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
  const MikktMesh* m = static_cast<const MikktMesh*>(ctx->m_pUserData);
  uint32_t v = MikktVertIndex(m, face, vert);
  out[0] = m->normals[v * 3 + 0];
  out[1] = m->normals[v * 3 + 1];
  out[2] = m->normals[v * 3 + 2];
}
void MikktGetTexCoord(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
  const MikktMesh* m = static_cast<const MikktMesh*>(ctx->m_pUserData);
  uint32_t v = MikktVertIndex(m, face, vert);
  out[0] = m->uvs[v * 2 + 0];
  out[1] = m->uvs[v * 2 + 1];
}
void MikktSetTSpace(const SMikkTSpaceContext* ctx, const float tangent[], float sign, int face, int vert) {
  MikktMesh* m = static_cast<MikktMesh*>(ctx->m_pUserData);
  uint32_t v = MikktVertIndex(m, face, vert);
  m->tanAccum[v * 3 + 0] += tangent[0];
  m->tanAccum[v * 3 + 1] += tangent[1];
  m->tanAccum[v * 3 + 2] += tangent[2];
  m->signAccum[v] += sign;
}
void PickPerpendicular(const float n[3], float t[3]) {
  float ax = n[0] < 0.0f ? -n[0] : n[0];
  float ay = n[1] < 0.0f ? -n[1] : n[1];
  float az = n[2] < 0.0f ? -n[2] : n[2];
  if (ax <= ay && ax <= az) {
    t[0] = 0.0f;
    t[1] = -n[2];
    t[2] = n[1];
  } else if (ay <= az) {
    t[0] = -n[2];
    t[1] = 0.0f;
    t[2] = n[0];
  } else {
    t[0] = -n[1];
    t[1] = n[0];
    t[2] = 0.0f;
  }
}
}
void OrthogonalizeTangents(const std::vector<float>& normals, std::vector<float>& tangents) {
  size_t n = normals.size() / 3;
  if (tangents.size() != n * 4) {
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    float nx = normals[i * 3 + 0];
    float ny = normals[i * 3 + 1];
    float nz = normals[i * 3 + 2];
    float tx = tangents[i * 4 + 0];
    float ty = tangents[i * 4 + 1];
    float tz = tangents[i * 4 + 2];
    float d = tx * nx + ty * ny + tz * nz;
    tx -= d * nx;
    ty -= d * ny;
    tz -= d * nz;
    float len = std::sqrt(tx * tx + ty * ty + tz * tz);
    if (len < 1e-6f) {
      float nrm[3] = { nx, ny, nz };
      float pick[3] = { 0, 0, 0 };
      PickPerpendicular(nrm, pick);
      tx = pick[0];
      ty = pick[1];
      tz = pick[2];
      len = std::sqrt(tx * tx + ty * ty + tz * tz);
      if (len < 1e-6f) {
        tx = 1.0f;
        ty = 0.0f;
        tz = 0.0f;
        len = 1.0f;
      }
    }
    tangents[i * 4 + 0] = tx / len;
    tangents[i * 4 + 1] = ty / len;
    tangents[i * 4 + 2] = tz / len;
    if (!(tangents[i * 4 + 3] == 1.0f || tangents[i * 4 + 3] == -1.0f)) {
      tangents[i * 4 + 3] = 1.0f;
    }
  }
}
bool GenerateTangents(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& uvs, const std::vector<uint32_t>& indices, std::vector<float>& tangentsOut) {
  tangentsOut.clear();
  size_t vertexCount = positions.size() / 3;
  if (vertexCount == 0 || positions.size() != vertexCount * 3 || normals.size() != positions.size() || uvs.size() != vertexCount * 2) {
    return false;
  }
  if (indices.size() < 3 || indices.size() % 3 != 0) {
    return false;
  }
  for (size_t i = 0; i < indices.size(); ++i) {
    if (indices[i] >= vertexCount) {
      return false;
    }
  }
  MikktMesh mesh;
  mesh.positions = positions.data();
  mesh.normals = normals.data();
  mesh.uvs = uvs.data();
  mesh.indices = indices.data();
  mesh.vertexCount = vertexCount;
  mesh.faceCount = indices.size() / 3;
  std::vector<float> tanAccum(vertexCount * 3, 0.0f);
  std::vector<float> signAccum(vertexCount, 0.0f);
  mesh.tanAccum = tanAccum.data();
  mesh.signAccum = signAccum.data();
  SMikkTSpaceInterface iface;
  iface.m_getNumFaces = MikktGetNumFaces;
  iface.m_getNumVerticesOfFace = MikktGetNumVerts;
  iface.m_getPosition = MikktGetPosition;
  iface.m_getNormal = MikktGetNormal;
  iface.m_getTexCoord = MikktGetTexCoord;
  iface.m_setTSpaceBasic = MikktSetTSpace;
  iface.m_setTSpace = nullptr;
  SMikkTSpaceContext ctx;
  ctx.m_pInterface = &iface;
  ctx.m_pUserData = &mesh;
  tangentsOut.resize(vertexCount * 4);
  if (genTangSpaceDefault(&ctx) == 0) {
    for (size_t i = 0; i < vertexCount; ++i) {
      tangentsOut[i * 4 + 0] = 0.0f;
      tangentsOut[i * 4 + 1] = 0.0f;
      tangentsOut[i * 4 + 2] = 0.0f;
      tangentsOut[i * 4 + 3] = 1.0f;
    }
    OrthogonalizeTangents(normals, tangentsOut);
    return true;
  }
  for (size_t i = 0; i < vertexCount; ++i) {
    tangentsOut[i * 4 + 0] = tanAccum[i * 3 + 0];
    tangentsOut[i * 4 + 1] = tanAccum[i * 3 + 1];
    tangentsOut[i * 4 + 2] = tanAccum[i * 3 + 2];
    tangentsOut[i * 4 + 3] = signAccum[i] < 0.0f ? -1.0f : 1.0f;
  }
  OrthogonalizeTangents(normals, tangentsOut);
  return true;
}
}
