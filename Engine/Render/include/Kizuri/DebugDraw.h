#pragma once
namespace Kizuri {
static const int LightSphereSegs = 72;
static const int LightConeSegs = 20;
static const int LightArrowSegs = 3;
static const int LightRectSegs = 5;
static const int LightSpherePts = 144;
static const int LightConePts = 40;
static const int LightArrowPts = 6;
bool ProjectWorldToScreen(const float view[16], const float proj[16], const float wpos[3], float viewX, float viewY, float viewW, float viewH, float& outX, float& outY);
void LightSpherePoints(const float center[3], float radius, float outPoints[144][3]);
void LightConePoints(const float apex[3], const float dir[3], float angleDeg, float length, float outPoints[40][3]);
void LightArrowPoints(const float origin[3], const float dir[3], float length, float outPoints[6][3]);
void LightRectPoints(const float center[3], const float normal[3], float halfW, float halfH, float outPoints[10][3]);
}
