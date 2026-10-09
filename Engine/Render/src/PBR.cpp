#include "Kizuri/PBR.h"
#include <cmath>
namespace Kizuri {
namespace {
void Norm3(float v[3]) {
  float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 1e-6f) {
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
  }
}
float NDF_GGX(float cosLh, float roughness) {
  float alpha = roughness * roughness;
  float alphaSq = alpha * alpha;
  float denom = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
  return alphaSq / (3.14159265f * denom * denom + 1e-6f);
}
float GA_SchlickG1(float cosTheta, float k) {
  return cosTheta / (cosTheta * (1.0f - k) + k + 1e-6f);
}
float GA_SchlickGGX(float cosLi, float cosLo, float roughness) {
  float r = roughness + 1.0f;
  float k = (r * r) / 8.0f;
  return GA_SchlickG1(cosLi, k) * GA_SchlickG1(cosLo, k);
}
float Smooth01(float e0, float e1, float x) {
  float t = (x - e0) / (e1 - e0);
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }
  return t * t * (3.0f - 2.0f * t);
}
void ShadeCore(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  const float lightColor[3],
  float scale,
  float outColor[3]) {
  float nn[3] = { N[0], N[1], N[2] };
  float vv[3] = { V[0], V[1], V[2] };
  float ll[3] = { L[0], L[1], L[2] };
  Norm3(nn);
  Norm3(vv);
  Norm3(ll);
  float hh[3] = { ll[0] + vv[0], ll[1] + vv[1], ll[2] + vv[2] };
  Norm3(hh);
  if (roughness < 0.04f) {
    roughness = 0.04f;
  }
  if (roughness > 1.0f) {
    roughness = 1.0f;
  }
  if (metallic < 0.0f) {
    metallic = 0.0f;
  }
  if (metallic > 1.0f) {
    metallic = 1.0f;
  }
  float cosLi = nn[0] * ll[0] + nn[1] * ll[1] + nn[2] * ll[2];
  float cosLo = nn[0] * vv[0] + nn[1] * vv[1] + nn[2] * vv[2];
  if (cosLi < 0.0f) {
    cosLi = 0.0f;
  }
  if (cosLo < 0.0f) {
    cosLo = 0.0f;
  }
  float cosLh = nn[0] * hh[0] + nn[1] * hh[1] + nn[2] * hh[2];
  if (cosLh < 0.0f) {
    cosLh = 0.0f;
  }
  float vh = vv[0] * hh[0] + vv[1] * hh[1] + vv[2] * hh[2];
  if (vh < 0.0f) {
    vh = 0.0f;
  }
  float F0die = 0.04f;
  float F0[3] = {
    F0die + (albedo[0] - F0die) * metallic,
    F0die + (albedo[1] - F0die) * metallic,
    F0die + (albedo[2] - F0die) * metallic
  };
  float f = std::pow(1.0f - vh, 5.0f);
  float F[3] = {
    F0[0] + (1.0f - F0[0]) * f,
    F0[1] + (1.0f - F0[1]) * f,
    F0[2] + (1.0f - F0[2]) * f
  };
  float D = NDF_GGX(cosLh, roughness);
  float G = GA_SchlickGGX(cosLi, cosLo, roughness);
  float denom = 4.0f * cosLi * cosLo + 1e-5f;
  float spec[3] = {
    (F[0] * D * G) / denom,
    (F[1] * D * G) / denom,
    (F[2] * D * G) / denom
  };
  float kd[3] = {
    (1.0f - F[0]) * (1.0f - metallic),
    (1.0f - F[1]) * (1.0f - metallic),
    (1.0f - F[2]) * (1.0f - metallic)
  };
  float diff[3] = {
    kd[0] * albedo[0] / 3.14159265f,
    kd[1] * albedo[1] / 3.14159265f,
    kd[2] * albedo[2] / 3.14159265f
  };
  outColor[0] = (diff[0] + spec[0]) * lightColor[0] * cosLi * scale;
  outColor[1] = (diff[1] + spec[1]) * lightColor[1] * cosLi * scale;
  outColor[2] = (diff[2] + spec[2]) * lightColor[2] * cosLi * scale;
}
}
void PBR_Directional(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  const float lightColor[3],
  float outColor[3]) {
  ShadeCore(albedo, roughness, metallic, N, V, L, lightColor, 1.0f, outColor);
}
void PBR_Point(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  float dist,
  float range,
  const float lightColor[3],
  float outColor[3]) {
  float r = range < 1e-3f ? 1e-3f : range;
  float q = dist * dist / (r * r);
  float att = q >= 1.0f ? 0.0f : (1.0f - q) * (1.0f - q);
  ShadeCore(albedo, roughness, metallic, N, V, L, lightColor, att, outColor);
}
void PBR_Spot(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  float dist,
  float range,
  const float spotDir[3],
  float cosOuter,
  float falloff,
  const float lightColor[3],
  float outColor[3]) {
  float r = range < 1e-3f ? 1e-3f : range;
  float q = dist * dist / (r * r);
  float att = q >= 1.0f ? 0.0f : (1.0f - q) * (1.0f - q);
  float sd[3] = { spotDir[0], spotDir[1], spotDir[2] };
  Norm3(sd);
  float cosT = L[0] * -sd[0] + L[1] * -sd[1] + L[2] * -sd[2];
  float w = falloff < 0.01f ? 0.01f : falloff;
  if (w > 0.5f) {
    w = 0.5f;
  }
  float spot = Smooth01(cosOuter, cosOuter + w, cosT);
  ShadeCore(albedo, roughness, metallic, N, V, L, lightColor, att * spot, outColor);
}
float ACESFilm(float x) {
  if (x <= 0.0f) {
    return 0.0f;
  }
  return (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
}
void SkyGradient(
  const float viewDir[3],
  const float sunDirTo[3],
  const float sunColor[3],
  float sunIntensity,
  float outColor[3]) {
  float vx = viewDir[0];
  float vy = viewDir[1];
  float vz = viewDir[2];
  float vl = std::sqrt(vx * vx + vy * vy + vz * vz);
  if (vl < 1e-6f) {
    vl = 1e-6f;
  }
  float dx = vx / vl;
  float dy = vy / vl;
  float dz = vz / vl;
  float sx = sunDirTo[0];
  float sy = sunDirTo[1];
  float sz = sunDirTo[2];
  float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
  if (sl < 1e-6f) {
    sl = 1e-6f;
  }
  sx /= sl;
  sy /= sl;
  sz /= sl;
  float dayness = sy * 3.0f + 0.3f;
  if (dayness < 0.0f) {
    dayness = 0.0f;
  }
  if (dayness > 1.0f) {
    dayness = 1.0f;
  }
  float upness = dy < 0.0f ? 0.0f : dy;
  if (upness > 1.0f) {
    upness = 1.0f;
  }
  float zen[3] = { 0.20f, 0.38f, 0.70f };
  float hor[3] = { 0.65f, 0.55f, 0.45f };
  float gnd[3] = { 0.10f, 0.09f, 0.08f };
  float zb = 0.15f + 0.85f * dayness;
  float pw = std::pow(upness, 0.5f);
  float sky[3];
  sky[0] = hor[0] + (zen[0] - hor[0]) * pw;
  sky[1] = hor[1] + (zen[1] - hor[1]) * pw;
  sky[2] = hor[2] + (zen[2] - hor[2]) * pw;
  float at = dy < 0.0f ? -dy : dy;
  float hb = std::exp(-at * 6.0f);
  sky[0] += (hor[0] - sky[0]) * hb * 0.5f;
  sky[1] += (hor[1] - sky[1]) * hb * 0.5f;
  sky[2] += (hor[2] - sky[2]) * hb * 0.5f;
  if (dy < 0.0f) {
    float gk = -dy * 3.0f;
    if (gk > 1.0f) {
      gk = 1.0f;
    }
    sky[0] += (gnd[0] - sky[0]) * gk;
    sky[1] += (gnd[1] - sky[1]) * gk;
    sky[2] += (gnd[2] - sky[2]) * gk;
  }
  float dim = (0.2f + 0.8f * dayness) * zb;
  sky[0] *= dim;
  sky[1] *= dim;
  sky[2] *= dim;
  float cosG = dx * sx + dy * sy + dz * sz;
  if (cosG < 0.0f) {
    cosG = 0.0f;
  }
  if (cosG > 1.0f) {
    cosG = 1.0f;
  }
  float glow = std::pow(cosG, 900.0f) * 4.0f + std::pow(cosG, 10.0f) * 0.25f;
  float sf = (0.15f + 0.85f * dayness) * sunIntensity;
  outColor[0] = sky[0] + sunColor[0] * glow * sf;
  outColor[1] = sky[1] + sunColor[1] * glow * sf;
  outColor[2] = sky[2] + sunColor[2] * glow * sf;
}
float SsaoTapOcclusion(
  const float pixelPos[3],
  const float samplePos[3],
  const float kernelPos[3],
  const float camPos[3],
  float radius,
  float bias) {
  float r = radius < 1e-4f ? 1e-4f : radius;
  float dx = samplePos[0] - pixelPos[0];
  float dy = samplePos[1] - pixelPos[1];
  float dz = samplePos[2] - pixelPos[2];
  float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (dist > r) {
    return 0.0f;
  }
  float sx = samplePos[0] - camPos[0];
  float sy = samplePos[1] - camPos[1];
  float sz = samplePos[2] - camPos[2];
  float kx = kernelPos[0] - camPos[0];
  float ky = kernelPos[1] - camPos[1];
  float kz = kernelPos[2] - camPos[2];
  float dcS = std::sqrt(sx * sx + sy * sy + sz * sz);
  float dcK = std::sqrt(kx * kx + ky * ky + kz * kz);
  if (dcS >= dcK - bias) {
    return 0.0f;
  }
  return 1.0f - dist / r;
}
float FogTransmittance(float density, float dist) {
  if (density <= 0.0f || dist <= 0.0f) {
    return 1.0f;
  }
  return std::exp(-density * dist);
}
}
