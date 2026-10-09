#include "Kizuri/DebugDraw.h"
#include <cmath>
namespace Kizuri {
namespace {
void MulRow(const float v[4], const float m[16], float out[4]) {
  out[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
  out[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
  out[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
  out[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
}
void Norm3(float v[3]) {
  float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l < 1e-6f) {
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 1.0f;
    return;
  }
  v[0] /= l;
  v[1] /= l;
  v[2] /= l;
}
void PerpBasis(const float dir[3], float u[3], float v[3]) {
  float ref[3] = { 0.0f, 1.0f, 0.0f };
  float d[3] = { dir[0], dir[1], dir[2] };
  Norm3(d);
  float dot = d[0] * ref[0] + d[1] * ref[1] + d[2] * ref[2];
  if (dot > 0.99f || dot < -0.99f) {
    ref[0] = 1.0f;
    ref[1] = 0.0f;
    ref[2] = 0.0f;
  }
  u[0] = ref[1] * d[2] - ref[2] * d[1];
  u[1] = ref[2] * d[0] - ref[0] * d[2];
  u[2] = ref[0] * d[1] - ref[1] * d[0];
  Norm3(u);
  v[0] = d[1] * u[2] - d[2] * u[1];
  v[1] = d[2] * u[0] - d[0] * u[2];
  v[2] = d[0] * u[1] - d[1] * u[0];
}
}
bool ProjectWorldToScreen(const float view[16], const float proj[16], const float wpos[3], float viewX, float viewY, float viewW, float viewH, float& outX, float& outY) {
  if (viewW < 1.0f || viewH < 1.0f) {
    return false;
  }
  float v[4] = { wpos[0], wpos[1], wpos[2], 1.0f };
  float c[4];
  MulRow(v, view, c);
  float q[4];
  MulRow(c, proj, q);
  if (q[3] <= 1e-6f) {
    return false;
  }
  float nx = q[0] / q[3];
  float ny = q[1] / q[3];
  outX = viewX + (nx * 0.5f + 0.5f) * viewW;
  outY = viewY + (1.0f - (ny * 0.5f + 0.5f)) * viewH;
  return true;
}
void LightSpherePoints(const float center[3], float radius, float outPoints[144][3]) {
  float r = radius > 0.0f ? radius : 0.0f;
  int k = 0;
  for (int plane = 0; plane < 3; ++plane) {
    for (int i = 0; i < 24; ++i) {
      float a0 = static_cast<float>(i) * 0.26179938779f;
      float a1 = static_cast<float>(i + 1) * 0.26179938779f;
      float p0[3] = { center[0], center[1], center[2] };
      float p1[3] = { center[0], center[1], center[2] };
      if (plane == 0) {
        p0[0] += r * cosf(a0);
        p0[1] += r * sinf(a0);
        p1[0] += r * cosf(a1);
        p1[1] += r * sinf(a1);
      } else if (plane == 1) {
        p0[0] += r * cosf(a0);
        p0[2] += r * sinf(a0);
        p1[0] += r * cosf(a1);
        p1[2] += r * sinf(a1);
      } else {
        p0[1] += r * cosf(a0);
        p0[2] += r * sinf(a0);
        p1[1] += r * cosf(a1);
        p1[2] += r * sinf(a1);
      }
      outPoints[k][0] = p0[0];
      outPoints[k][1] = p0[1];
      outPoints[k][2] = p0[2];
      ++k;
      outPoints[k][0] = p1[0];
      outPoints[k][1] = p1[1];
      outPoints[k][2] = p1[2];
      ++k;
    }
  }
}
void LightConePoints(const float apex[3], const float dir[3], float angleDeg, float length, float outPoints[40][3]) {
  float a = angleDeg;
  if (a < 2.0f) {
    a = 2.0f;
  }
  if (a > 170.0f) {
    a = 170.0f;
  }
  float len = length > 0.0f ? length : 0.0f;
  float d[3] = { dir[0], dir[1], dir[2] };
  Norm3(d);
  float u[3];
  float v[3];
  PerpBasis(d, u, v);
  float half = a * 0.5f * 0.01745329252f;
  float rr = tanf(half) * len;
  float cx = apex[0] + d[0] * len;
  float cy = apex[1] + d[1] * len;
  float cz = apex[2] + d[2] * len;
  int k = 0;
  for (int i = 0; i < 16; ++i) {
    float a0 = static_cast<float>(i) * 0.39269908169f;
    float a1 = static_cast<float>(i + 1) * 0.39269908169f;
    outPoints[k][0] = cx + (u[0] * cosf(a0) + v[0] * sinf(a0)) * rr;
    outPoints[k][1] = cy + (u[1] * cosf(a0) + v[1] * sinf(a0)) * rr;
    outPoints[k][2] = cz + (u[2] * cosf(a0) + v[2] * sinf(a0)) * rr;
    ++k;
    outPoints[k][0] = cx + (u[0] * cosf(a1) + v[0] * sinf(a1)) * rr;
    outPoints[k][1] = cy + (u[1] * cosf(a1) + v[1] * sinf(a1)) * rr;
    outPoints[k][2] = cz + (u[2] * cosf(a1) + v[2] * sinf(a1)) * rr;
    ++k;
  }
  for (int g = 0; g < 4; ++g) {
    float ag = static_cast<float>(g) * 1.57079632679f;
    outPoints[k][0] = apex[0];
    outPoints[k][1] = apex[1];
    outPoints[k][2] = apex[2];
    ++k;
    outPoints[k][0] = cx + (u[0] * cosf(ag) + v[0] * sinf(ag)) * rr;
    outPoints[k][1] = cy + (u[1] * cosf(ag) + v[1] * sinf(ag)) * rr;
    outPoints[k][2] = cz + (u[2] * cosf(ag) + v[2] * sinf(ag)) * rr;
    ++k;
  }
}
void LightArrowPoints(const float origin[3], const float dir[3], float length, float outPoints[6][3]) {  float len = length > 0.0f ? length : 0.0f;
  float d[3] = { dir[0], dir[1], dir[2] };
  Norm3(d);
  float u[3];
  float v[3];
  PerpBasis(d, u, v);
  float tx = origin[0] + d[0] * len;
  float ty = origin[1] + d[1] * len;
  float tz = origin[2] + d[2] * len;
  float hl = len * 0.25f;
  float hw = len * 0.12f;
  outPoints[0][0] = origin[0];
  outPoints[0][1] = origin[1];
  outPoints[0][2] = origin[2];
  outPoints[1][0] = tx;
  outPoints[1][1] = ty;
  outPoints[1][2] = tz;
  outPoints[2][0] = tx;
  outPoints[2][1] = ty;
  outPoints[2][2] = tz;
  outPoints[3][0] = tx - d[0] * hl + u[0] * hw;
  outPoints[3][1] = ty - d[1] * hl + u[1] * hw;
  outPoints[3][2] = tz - d[2] * hl + u[2] * hw;
  outPoints[4][0] = tx;
  outPoints[4][1] = ty;
  outPoints[4][2] = tz;
  outPoints[5][0] = tx - d[0] * hl - u[0] * hw;
  outPoints[5][1] = ty - d[1] * hl - u[1] * hw;
  outPoints[5][2] = tz - d[2] * hl - u[2] * hw;
}
void LightRectPoints(const float center[3], const float normal[3], float halfW, float halfH, float outPoints[10][3]) {
  float n[3] = { normal[0], normal[1], normal[2] };
  Norm3(n);
  float u[3];
  float v[3];
  PerpBasis(n, u, v);
  float hw = halfW > 0.0f ? halfW : 0.5f;
  float hh = halfH > 0.0f ? halfH : 0.5f;
  float c[4][3];
  for (int k = 0; k < 4; ++k) {
    float su = (k == 0 || k == 3) ? -hw : hw;
    float sv = (k < 2) ? -hh : hh;
    c[k][0] = center[0] + u[0] * su + v[0] * sv;
    c[k][1] = center[1] + u[1] * su + v[1] * sv;
    c[k][2] = center[2] + u[2] * su + v[2] * sv;
  }
  for (int k = 0; k < 4; ++k) {
    int n2 = (k + 1) % 4;
    outPoints[k * 2][0] = c[k][0];
    outPoints[k * 2][1] = c[k][1];
    outPoints[k * 2][2] = c[k][2];
    outPoints[k * 2 + 1][0] = c[n2][0];
    outPoints[k * 2 + 1][1] = c[n2][1];
    outPoints[k * 2 + 1][2] = c[n2][2];
  }
  outPoints[8][0] = center[0];
  outPoints[8][1] = center[1];
  outPoints[8][2] = center[2];
  outPoints[9][0] = center[0] + n[0];
  outPoints[9][1] = center[1] + n[1];
  outPoints[9][2] = center[2] + n[2];
}
}
