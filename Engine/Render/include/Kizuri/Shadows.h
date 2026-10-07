#pragma once
namespace Kizuri {
static const int ShadowAtlasSize = 4096;
static const int ShadowTileSize = 2048;
static const int ShadowTileCount = 4;
static const int ShadowCubeSize = 512;
static const int MaxShadowCascades = 4;
static const int MaxShadowSpots = 4;
static const int MaxShadowSlots = 4;
void ShadowSplitDepths(float nearZ, float farZ, int cascadeCount, float lambda, float outSplits[5]);
void ShadowFrustumCorners(const float camPos[3], const float camFwd[3], const float camRight[3], const float camUp[3], float fovY, float aspect, float nearD, float farD, float outCorners[8][3]);
void ShadowSunMatrix(const float sunDir[3], const float camPos[3], const float camFwd[3], const float camRight[3], const float camUp[3], float fovY, float aspect, float splitNear, float splitFar, int tileSize, float outVP[16], float& outNear, float& outFar, float& outExtent);
void ShadowSpotMatrix(const float pos[3], const float dir[3], float angleDeg, float radius, float outVP[16]);
void ShadowPointFaces(const float pos[3], float radius, float outVP[6][16]);
struct ShadowAtlasPlan {
  int dirTileStart[4];
  int spotTiles[4];
  int spotMapped;
};
ShadowAtlasPlan PlanShadowAtlas(const int* dirCascades, int dirCount, int spotCount);
void ShadowTileOrigin(int tile, float& tileX, float& tileY);
void ShadowTileUV(float ndcX, float ndcY, int tile, float tileFrac, float& u, float& v);
float ShadowLinearizeDepth(float ndcZ, float nearZ, float farZ);
float PCSSPenumbraWidth(float receiverDist, float blockerDist, float effectiveSize);
float PCSSOrthoUVScale(float orthoExtent, float tileFrac);
float PCSSPerspUVScale(float tanHalfFov, float receiverDist, float tileFrac);
void MulViewProj(const float v[16], const float p[16], float vp[16]);
void ExtractFrustumPlanes(const float vp[16], float planes[6][4]);
bool CullAABB(const float planes[6][4], const float center[3], const float extents[3]);
void TransformAABB(const float world[16], const float inMin[3], const float inMax[3], float outCenter[3], float outExtents[3]);
}
