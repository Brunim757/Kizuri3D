#pragma once
namespace Kizuri {
enum class QualityLevel {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3
};
struct QualitySettings {
  bool ssao = true;
  bool ssr = true;
  bool contact = true;
  bool ssgi = false;
  bool dof = false;
  bool motionBlur = false;
  bool fxaa = true;
  bool fog = true;
  float bloom = 0.8f;
  float renderScale = 1.0f;
};
inline QualitySettings ApplyQualityPreset(QualityLevel level) {
  QualitySettings q;
  if (level == QualityLevel::Low) {
    q.ssao = true;
    q.ssr = false;
    q.contact = false;
    q.ssgi = false;
    q.dof = false;
    q.motionBlur = false;
    q.fxaa = true;
    q.fog = true;
    q.bloom = 0.0f;
    q.renderScale = 0.75f;
    return q;
  }
  if (level == QualityLevel::Medium) {
    q.ssao = true;
    q.ssr = true;
    q.contact = true;
    q.ssgi = false;
    q.dof = false;
    q.motionBlur = false;
    q.fxaa = true;
    q.fog = true;
    q.bloom = 0.5f;
    q.renderScale = 0.85f;
    return q;
  }
  if (level == QualityLevel::Ultra) {
    q.ssao = true;
    q.ssr = true;
    q.contact = true;
    q.ssgi = true;
    q.dof = true;
    q.motionBlur = true;
    q.fxaa = true;
    q.fog = true;
    q.bloom = 1.0f;
    q.renderScale = 1.0f;
    return q;
  }
  q.ssao = true;
  q.ssr = true;
  q.contact = true;
  q.ssgi = true;
  q.dof = true;
  q.motionBlur = false;
  q.fxaa = true;
  q.fog = true;
  q.bloom = 0.8f;
  q.renderScale = 1.0f;
  return q;
}
}
