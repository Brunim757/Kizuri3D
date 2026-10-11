#pragma once
namespace Kizuri {
enum class QualityLevel {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3,
  Custom = 4
};
struct QualitySettings {
  bool ssao = true;
  int aoMode = 1;
  bool ssr = true;
  bool ssrTemporal = true;
  bool contact = true;
  bool ssgi = false;
  bool dof = false;
  bool motionBlur = false;
  bool fxaa = true;
  bool taa = false;
  float taaSharp = 0.25f;
  bool fog = true;
  float bloom = 0.8f;
  float renderScale = 1.0f;
  int shadowFilter = 0;
  int vsmQuality = 1;
  float shadowRange = 0.0f;
  bool dynRes = false;
  bool occ = true;
  bool shadowAuto = false;
  bool godRays = false;
};
inline QualitySettings ApplyQualityPreset(QualityLevel level) {
  QualitySettings q;
  if (level == QualityLevel::Low) {
    q.ssao = true;
    q.aoMode = 1;
    q.ssr = false;
    q.ssrTemporal = false;
    q.contact = false;
    q.ssgi = false;
    q.dof = false;
    q.motionBlur = false;
    q.fxaa = true;
    q.taa = false;
    q.fog = true;
    q.bloom = 0.0f;
    q.renderScale = 0.75f;
    q.shadowFilter = 1;
    q.vsmQuality = 2;
    q.shadowRange = 60.0f;
    q.dynRes = true;
    q.occ = true;
    q.shadowAuto = true;
    q.godRays = false;
    return q;
  }
  if (level == QualityLevel::Medium) {
    q.ssao = true;
    q.aoMode = 1;
    q.ssr = true;
    q.ssrTemporal = true;
    q.contact = true;
    q.ssgi = false;
    q.dof = false;
    q.motionBlur = false;
    q.fxaa = true;
    q.taa = true;
    q.fog = true;
    q.bloom = 0.5f;
    q.renderScale = 0.85f;
    q.shadowFilter = 0;
    q.vsmQuality = 1;
    q.shadowRange = 100.0f;
    q.dynRes = false;
    q.occ = true;
    q.shadowAuto = true;
    q.godRays = false;
    return q;
  }
  if (level == QualityLevel::Ultra) {
    q.ssao = true;
    q.aoMode = 3;
    q.ssr = true;
    q.ssrTemporal = true;
    q.contact = true;
    q.ssgi = true;
    q.dof = true;
    q.motionBlur = true;
    q.fxaa = true;
    q.taa = true;
    q.fog = true;
    q.bloom = 1.0f;
    q.renderScale = 1.0f;
    q.shadowFilter = 3;
    q.vsmQuality = 0;
    q.shadowRange = 0.0f;
    q.dynRes = false;
    q.occ = true;
    q.shadowAuto = false;
    q.godRays = true;
    return q;
  }
  q.ssao = true;
  q.aoMode = 2;
  q.ssr = true;
  q.ssrTemporal = true;
  q.contact = true;
  q.ssgi = true;
  q.dof = true;
  q.motionBlur = false;
  q.fxaa = true;
  q.taa = true;
  q.fog = true;
  q.bloom = 0.8f;
  q.renderScale = 1.0f;
  q.shadowFilter = 2;
  q.vsmQuality = 1;
  q.shadowRange = 0.0f;
  q.dynRes = false;
  q.occ = true;
  q.shadowAuto = false;
  q.godRays = true;
  return q;
}
}
