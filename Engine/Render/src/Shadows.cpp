#include "Kizuri/Shadows.h"
#include <DirectXMath.h>
#include <cmath>
#include <cstring>
namespace Kizuri {
void ShadowSplitDepths(float nearZ, float farZ, int cascadeCount, float lambda, float outSplits[5]) {
  if (cascadeCount < 1) {
    cascadeCount = 1;
  }
  if (cascadeCount > 4) {
    cascadeCount = 4;
  }
  if (nearZ < 0.01f) {
    nearZ = 0.01f;
  }
  if (farZ <= nearZ) {
    farZ = nearZ + 1.0f;
  }
  if (lambda < 0.0f) {
    lambda = 0.0f;
  }
  if (lambda > 1.0f) {
    lambda = 1.0f;
  }
  float ratio = farZ / nearZ;
  for (int i = 0; i <= cascadeCount; ++i) {
    float f = static_cast<float>(i) / static_cast<float>(cascadeCount);
    float logSplit = nearZ * std::pow(ratio, f);
    float uniSplit = nearZ + (farZ - nearZ) * f;
    outSplits[i] = lambda * logSplit + (1.0f - lambda) * uniSplit;
  }
}
void ShadowFrustumCorners(const float camPos[3], const float camFwd[3], const float camRight[3], const float camUp[3], float fovY, float aspect, float nearD, float farD, float outCorners[8][3]) {
  float tanV = std::tan(fovY * 0.5f);
  float tanH = tanV * aspect;
  if (tanV < 0.001f) {
    tanV = 0.001f;
  }
  if (tanH < 0.001f) {
    tanH = 0.001f;
  }
  if (nearD < 0.01f) {
    nearD = 0.01f;
  }
  if (farD <= nearD) {
    farD = nearD + 1.0f;
  }
  float depths[2] = { nearD, farD };
  for (int s = 0; s < 2; ++s) {
    float d = depths[s];
    float hw = tanH * d;
    float hh = tanV * d;
    for (int c = 0; c < 4; ++c) {
      float sx = (c == 0 || c == 3) ? -1.0f : 1.0f;
      float sy = (c < 2) ? -1.0f : 1.0f;
      int k = s * 4 + c;
      outCorners[k][0] = camPos[0] + camFwd[0] * d + camRight[0] * (sx * hw) + camUp[0] * (sy * hh);
      outCorners[k][1] = camPos[1] + camFwd[1] * d + camRight[1] * (sx * hw) + camUp[1] * (sy * hh);
      outCorners[k][2] = camPos[2] + camFwd[2] * d + camRight[2] * (sx * hw) + camUp[2] * (sy * hh);
    }
  }
}
void ShadowSunMatrix(const float sunDir[3], const float camPos[3], const float camFwd[3], const float camRight[3], const float camUp[3], float fovY, float aspect, float splitNear, float splitFar, int tileSize, float outVP[16], float& outNear, float& outFar, float& outExtent) {
  using namespace DirectX;
  float corners[8][3];
  ShadowFrustumCorners(camPos, camFwd, camRight, camUp, fovY, aspect, splitNear, splitFar, corners);
  XMVECTOR c = XMVectorZero();
  for (int i = 0; i < 8; ++i) {
    c += XMVectorSet(corners[i][0], corners[i][1], corners[i][2], 0.0f);
  }
  c /= 8.0f;
  XMVECTOR L = XMVectorSet(sunDir[0], sunDir[1], sunDir[2], 0.0f);
  if (XMVectorGetX(XMVector3LengthSq(L)) < 1e-8f) {
    L = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
  }
  L = XMVector3Normalize(L);
  float radius = 0.0f;
  for (int i = 0; i < 8; ++i) {
    XMVECTOR p = XMVectorSet(corners[i][0], corners[i][1], corners[i][2], 0.0f);
    float d = XMVectorGetX(XMVector3Length(p - c));
    if (d > radius) {
      radius = d;
    }
  }
  if (radius < 0.01f) {
    radius = 0.01f;
  }
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  if (std::fabs(XMVectorGetX(XMVector3Dot(L, up))) > 0.99f) {
    up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
  }
  XMVECTOR eye = c - L * (radius * 2.0f);
  XMMATRIX view = XMMatrixLookAtLH(eye, c, up);
  float maxExtent = 0.0f;
  for (int i = 0; i < 8; ++i) {
    XMVECTOR p = XMVectorSet(corners[i][0], corners[i][1], corners[i][2], 1.0f);
    XMVECTOR q = XMVector3TransformCoord(p, view);
    float ax = std::fabs(XMVectorGetX(q));
    float ay = std::fabs(XMVectorGetY(q));
    if (ax > maxExtent) {
      maxExtent = ax;
    }
    if (ay > maxExtent) {
      maxExtent = ay;
    }
  }
  if (maxExtent < 0.01f) {
    maxExtent = 0.01f;
  }
  int ts = tileSize > 0 ? tileSize : 2048;
  float texel = (maxExtent * 2.0f) / static_cast<float>(ts);
  XMVECTOR cLS = XMVector3TransformCoord(c, view);
  float sx = std::floor(XMVectorGetX(cLS) / texel) * texel;
  float sy = std::floor(XMVectorGetY(cLS) / texel) * texel;
  float nearP = radius * 0.5f;
  float farP = radius * 4.0f;
  outNear = nearP;
  outFar = farP;
  outExtent = maxExtent;
  XMMATRIX proj = XMMatrixOrthographicOffCenterLH(sx - maxExtent, sx + maxExtent, sy - maxExtent, sy + maxExtent, nearP, farP);
  XMMATRIX vp = view * proj;
  XMFLOAT4X4 m;
  XMStoreFloat4x4(&m, vp);
  std::memcpy(outVP, &m.m[0][0], sizeof(float) * 16);
}
void ShadowSpotMatrix(const float pos[3], const float dir[3], float angleDeg, float radius, float outVP[16]) {
  using namespace DirectX;
  if (angleDeg < 1.0f) {
    angleDeg = 1.0f;
  }
  if (angleDeg > 170.0f) {
    angleDeg = 170.0f;
  }
  if (radius < 1.0f) {
    radius = 1.0f;
  }
  XMVECTOR D = XMVectorSet(dir[0], dir[1], dir[2], 0.0f);
  if (XMVectorGetX(XMVector3LengthSq(D)) < 1e-8f) {
    D = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
  }
  D = XMVector3Normalize(D);
  XMVECTOR P = XMVectorSet(pos[0], pos[1], pos[2], 0.0f);
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  if (std::fabs(XMVectorGetX(XMVector3Dot(D, up))) > 0.99f) {
    up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
  }
  XMMATRIX view = XMMatrixLookAtLH(P, P + D, up);
  float fov = angleDeg * 0.01745329252f;
  XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, 1.0f, 0.5f, radius);
  XMMATRIX vp = view * proj;
  XMFLOAT4X4 m;
  XMStoreFloat4x4(&m, vp);
  std::memcpy(outVP, &m.m[0][0], sizeof(float) * 16);
}
void ShadowPointFaces(const float pos[3], float radius, float outVP[6][16]) {
  using namespace DirectX;
  if (radius < 1.0f) {
    radius = 1.0f;
  }
  XMVECTOR P = XMVectorSet(pos[0], pos[1], pos[2], 0.0f);
  float dirs[6][3] = {
    { 1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, -1.0f }
  };
  float ups[6][3] = {
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f }
  };
  XMMATRIX proj = XMMatrixPerspectiveFovLH(1.57079632679f, 1.0f, 0.5f, radius);
  for (int f = 0; f < 6; ++f) {
    XMVECTOR D = XMVectorSet(dirs[f][0], dirs[f][1], dirs[f][2], 0.0f);
    XMVECTOR U = XMVectorSet(ups[f][0], ups[f][1], ups[f][2], 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(P, P + D, U);
    XMMATRIX vp = view * proj;
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, vp);
    std::memcpy(outVP[f], &m.m[0][0], sizeof(float) * 16);
  }
}
ShadowAtlasPlan PlanShadowAtlas(const int* dirCascades, int dirCount, int spotCount) {
  ShadowAtlasPlan plan;
  plan.spotMapped = 0;
  for (int i = 0; i < 4; ++i) {
    plan.dirTileStart[i] = -1;
    plan.spotTiles[i] = -1;
  }
  int tile = 0;
  int dc = dirCount;
  if (dc < 0) {
    dc = 0;
  }
  if (dc > 4) {
    dc = 4;
  }
  for (int i = 0; i < dc; ++i) {
    int need = dirCascades != nullptr ? dirCascades[i] : 0;
    if (need < 0) {
      need = 0;
    }
    if (need > 4) {
      need = 4;
    }
    if (need == 0 || tile + need > ShadowTileCount) {
      continue;
    }
    plan.dirTileStart[i] = tile;
    tile += need;
  }
  int sc = spotCount;
  if (sc < 0) {
    sc = 0;
  }
  if (sc > 4) {
    sc = 4;
  }
  for (int i = 0; i < sc && tile < ShadowTileCount; ++i) {
    plan.spotTiles[plan.spotMapped] = tile;
    ++plan.spotMapped;
    ++tile;
  }
  return plan;
}
void ShadowTileOrigin(int tile, float& tileX, float& tileY) {
  int t = tile;
  if (t < 0) {
    t = 0;
  }
  if (t >= ShadowTileCount) {
    t = ShadowTileCount - 1;
  }
  tileX = static_cast<float>(t % 2);
  tileY = static_cast<float>(t / 2);
}
void ShadowTileUV(float ndcX, float ndcY, int tile, float tileFrac, float& u, float& v) {
  float tx = 0.0f;
  float ty = 0.0f;
  ShadowTileOrigin(tile, tx, ty);
  float f = tileFrac;
  if (f < 0.0f) {
    f = 0.0f;
  }
  if (f > 1.0f) {
    f = 1.0f;
  }
  u = (ndcX * 0.5f + 0.5f) * 0.5f * f + tx * 0.5f;
  v = (0.5f - ndcY * 0.5f) * 0.5f * f + ty * 0.5f;
}
float ShadowLinearizeDepth(float ndcZ, float nearZ, float farZ) {
  if (nearZ < 0.01f) {
    nearZ = 0.01f;
  }
  if (farZ <= nearZ) {
    farZ = nearZ + 1.0f;
  }
  float z = ndcZ;
  if (z < 0.0f) {
    z = 0.0f;
  }
  if (z > 1.0f) {
    z = 1.0f;
  }
  float denom = 1.0f - z * (farZ - nearZ) / farZ;
  if (denom < 1e-6f) {
    return farZ;
  }
  return nearZ / denom;
}
float PCSSPenumbraWidth(float receiverDist, float blockerDist, float effectiveSize) {
  if (effectiveSize <= 0.0f || blockerDist <= 1e-4f) {
    return 0.0f;
  }
  float w = (receiverDist - blockerDist) * effectiveSize / blockerDist;
  if (w < 0.0f) {
    return 0.0f;
  }
  return w;
}
float PCSSOrthoUVScale(float orthoExtent, float tileFrac) {
  if (orthoExtent < 1e-4f) {
    return 0.0f;
  }
  float f = tileFrac;
  if (f < 0.0f) {
    f = 0.0f;
  }
  if (f > 1.0f) {
    f = 1.0f;
  }
  return 0.5f * f / (orthoExtent * 2.0f);
}
float PCSSPerspUVScale(float tanHalfFov, float receiverDist, float tileFrac) {
  if (tanHalfFov < 1e-4f || receiverDist < 1e-4f) {
    return 0.0f;
  }
  float f = tileFrac;
  if (f < 0.0f) {
    f = 0.0f;
  }
  if (f > 1.0f) {
    f = 1.0f;
  }
  return 0.5f * f / (tanHalfFov * receiverDist * 2.0f);
}
void MulViewProj(const float v[16], const float p[16], float vp[16]) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      vp[i * 4 + j] = v[i * 4 + 0] * p[0 * 4 + j] + v[i * 4 + 1] * p[1 * 4 + j] + v[i * 4 + 2] * p[2 * 4 + j] + v[i * 4 + 3] * p[3 * 4 + j];
    }
  }
}
void ExtractFrustumPlanes(const float vp[16], float planes[6][4]) {
  const float* r0 = vp + 0;
  const float* r1 = vp + 4;
  const float* r2 = vp + 8;
  const float* r3 = vp + 12;
  float raw[6][4] = {
    { r3[0] + r0[0], r3[1] + r0[1], r3[2] + r0[2], r3[3] + r0[3] },
    { r3[0] - r0[0], r3[1] - r0[1], r3[2] - r0[2], r3[3] - r0[3] },
    { r3[0] + r1[0], r3[1] + r1[1], r3[2] + r1[2], r3[3] + r1[3] },
    { r3[0] - r1[0], r3[1] - r1[1], r3[2] - r1[2], r3[3] - r1[3] },
    { r2[0], r2[1], r2[2], r2[3] },
    { r3[0] - r2[0], r3[1] - r2[1], r3[2] - r2[2], r3[3] - r2[3] }
  };
  for (int i = 0; i < 6; ++i) {
    float l = sqrtf(raw[i][0] * raw[i][0] + raw[i][1] * raw[i][1] + raw[i][2] * raw[i][2]);
    if (l < 1e-9f) {
      planes[i][0] = 0.0f;
      planes[i][1] = 0.0f;
      planes[i][2] = 0.0f;
      planes[i][3] = 0.0f;
      continue;
    }
    planes[i][0] = raw[i][0] / l;
    planes[i][1] = raw[i][1] / l;
    planes[i][2] = raw[i][2] / l;
    planes[i][3] = raw[i][3] / l;
  }
}
bool CullAABB(const float planes[6][4], const float center[3], const float extents[3]) {
  for (int i = 0; i < 6; ++i) {
    float d = planes[i][0] * center[0] + planes[i][1] * center[1] + planes[i][2] * center[2] + planes[i][3];
    float nx = planes[i][0] < 0.0f ? -planes[i][0] : planes[i][0];
    float ny = planes[i][1] < 0.0f ? -planes[i][1] : planes[i][1];
    float nz = planes[i][2] < 0.0f ? -planes[i][2] : planes[i][2];
    float r = nx * extents[0] + ny * extents[1] + nz * extents[2];
    if (d < -r) {
      return true;
    }
  }
  return false;
}
void TransformAABB(const float world[16], const float inMin[3], const float inMax[3], float outCenter[3], float outExtents[3]) {
  float c[3] = { (inMin[0] + inMax[0]) * 0.5f, (inMin[1] + inMax[1]) * 0.5f, (inMin[2] + inMax[2]) * 0.5f };
  float e[3] = { (inMax[0] - inMin[0]) * 0.5f, (inMax[1] - inMin[1]) * 0.5f, (inMax[2] - inMin[2]) * 0.5f };
  outCenter[0] = c[0] * world[0] + c[1] * world[4] + c[2] * world[8] + world[12];
  outCenter[1] = c[0] * world[1] + c[1] * world[5] + c[2] * world[9] + world[13];
  outCenter[2] = c[0] * world[2] + c[1] * world[6] + c[2] * world[10] + world[14];
  for (int i = 0; i < 3; ++i) {
    float a0 = world[0 * 4 + i] < 0.0f ? -world[0 * 4 + i] : world[0 * 4 + i];
    float a1 = world[1 * 4 + i] < 0.0f ? -world[1 * 4 + i] : world[1 * 4 + i];
    float a2 = world[2 * 4 + i] < 0.0f ? -world[2 * 4 + i] : world[2 * 4 + i];
    outExtents[i] = a0 * e[0] + a1 * e[1] + a2 * e[2];
  }
}
}
