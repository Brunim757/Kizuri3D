#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Assets/LightmapBaker.h"
#include "Kizuri/QualityPreset.h"
#include "FileDialog.h"
#include "Kizuri/DebugDraw.h"
#include "Kizuri/Input.h"
#include "Kizuri/Picking.h"
#include <windows.h>
#include <shellapi.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstring>
#include <cmath>
#include <filesystem>
namespace Kizuri {
uint32_t ResolveMaterialSlot(AssetDatabase& assets, const std::string& guid, uint32_t bit, RHITexture& out) {
  out = 0;
  if (guid.empty()) {
    return 0;
  }
  const TextureRecord* tr = assets.GetTexByGuid(guid);
  if (tr == nullptr || !tr->loaded || tr->gpu == 0) {
    return 0;
  }
  if (tr->selectedLevels <= 0 || tr->residentLevels < tr->selectedLevels) {
    return 0;
  }
  out = tr->gpu;
  return bit;
}
void MaterialDataFromMesh(const MeshMaterialData& mm, MaterialData& m) {
  m.albedo[0] = mm.albedo[0];
  m.albedo[1] = mm.albedo[1];
  m.albedo[2] = mm.albedo[2];
  m.alpha = mm.alpha;
  m.metallic = mm.metallic;
  m.roughness = mm.roughness;
  m.albedoTex = mm.albedoTexGuid;
  m.normalTex = mm.normalTexGuid;
  m.ormTex = mm.ormTexGuid;
  m.heightTex = mm.heightTexGuid;
  m.emissive[0] = mm.emissive[0];
  m.emissive[1] = mm.emissive[1];
  m.emissive[2] = mm.emissive[2];
  m.emissiveStrength = mm.emissiveStrength;
  m.emissiveTex = mm.emissiveTexGuid;
  m.unlit = mm.unlit;
  m.doubleSided = mm.doubleSided;
  m.alphaMode = mm.alphaMode;
  m.alphaCutoff = mm.alphaCutoff;
  for (int s = 0; s < 5; ++s) {
    m.xforms[s].offsetU = mm.xforms[s].offsetU;
    m.xforms[s].offsetV = mm.xforms[s].offsetV;
    m.xforms[s].scaleU = mm.xforms[s].scaleU;
    m.xforms[s].scaleV = mm.xforms[s].scaleV;
    m.xforms[s].rotation = mm.xforms[s].rotation;
    m.xforms[s].uvSet = mm.xforms[s].uvSet;
  }
  m.ior = mm.ior;
  m.hasSpecular = mm.hasSpecular;
  m.specularColor[0] = mm.specularColor[0];
  m.specularColor[1] = mm.specularColor[1];
  m.specularColor[2] = mm.specularColor[2];
  m.specularStrength = mm.specularStrength;
  m.specularTex = mm.specularTexGuid;
  m.specularColorTex = mm.specularColorTexGuid;
  m.clearcoat = mm.clearcoat;
  m.clearcoatRoughness = mm.clearcoatRoughness;
  m.clearcoatTex = mm.clearcoatTexGuid;
  m.clearcoatRoughnessTex = mm.clearcoatRoughnessTexGuid;
  m.clearcoatNormalTex = mm.clearcoatNormalTexGuid;
  m.sheenColor[0] = mm.sheenColor[0];
  m.sheenColor[1] = mm.sheenColor[1];
  m.sheenColor[2] = mm.sheenColor[2];
  m.sheenRoughness = mm.sheenRoughness;
  m.sheenColorTex = mm.sheenColorTexGuid;
  m.sheenRoughnessTex = mm.sheenRoughnessTexGuid;
  m.anisotropy = mm.anisotropy;
  m.anisotropyRotation = mm.anisotropyRotation;
  m.anisotropyTex = mm.anisotropyTexGuid;
  m.iridescence = mm.iridescence;
  m.iridescenceIor = mm.iridescenceIor;
  m.iridescenceThicknessMin = mm.iridescenceThicknessMin;
  m.iridescenceThicknessMax = mm.iridescenceThicknessMax;
  m.iridescenceTex = mm.iridescenceTexGuid;
  m.iridescenceThicknessTex = mm.iridescenceThicknessTexGuid;
  m.transmission = mm.transmission;
  m.transmissionTex = mm.transmissionTexGuid;
  m.thickness = mm.thickness;
  m.thicknessTex = mm.thicknessTexGuid;
  m.attenuationColor[0] = mm.attenuationColor[0];
  m.attenuationColor[1] = mm.attenuationColor[1];
  m.attenuationColor[2] = mm.attenuationColor[2];
  m.attenuationDistance = mm.attenuationDistance;
  m.dispersion = mm.dispersion;
}
void FillDeferredFromEntity(AssetDatabase& assets, const MaterialData& m, DeferredMaterial& partMat) {
  partMat.albedo[0] = m.albedo[0];
  partMat.albedo[1] = m.albedo[1];
  partMat.albedo[2] = m.albedo[2];
  partMat.alpha = m.alpha;
  partMat.roughness = m.roughness;
  partMat.metallic = m.metallic;
  partMat.emissive[0] = m.emissive[0];
  partMat.emissive[1] = m.emissive[1];
  partMat.emissive[2] = m.emissive[2];
  partMat.emissiveStrength = m.emissiveStrength;
  partMat.unlit = m.unlit;
  partMat.doubleSided = m.doubleSided;
  partMat.alphaMode = m.alphaMode;
  partMat.alphaCutoff = m.alphaCutoff;
  for (int s = 0; s < 5; ++s) {
    partMat.xforms[s].offsetU = m.xforms[s].offsetU;
    partMat.xforms[s].offsetV = m.xforms[s].offsetV;
    partMat.xforms[s].scaleU = m.xforms[s].scaleU;
    partMat.xforms[s].scaleV = m.xforms[s].scaleV;
    partMat.xforms[s].rotation = m.xforms[s].rotation;
    partMat.xforms[s].uvSet = m.xforms[s].uvSet;
  }
  partMat.ior = m.ior;
  partMat.hasSpecular = m.hasSpecular;
  partMat.specularColor[0] = m.specularColor[0];
  partMat.specularColor[1] = m.specularColor[1];
  partMat.specularColor[2] = m.specularColor[2];
  partMat.specularStrength = m.specularStrength;
  partMat.clearcoat = m.clearcoat;
  partMat.clearcoatRoughness = m.clearcoatRoughness;
  partMat.sheenColor[0] = m.sheenColor[0];
  partMat.sheenColor[1] = m.sheenColor[1];
  partMat.sheenColor[2] = m.sheenColor[2];
  partMat.sheenRoughness = m.sheenRoughness;
  partMat.anisotropy = m.anisotropy;
  partMat.anisotropyRotation = m.anisotropyRotation;
  partMat.iridescence = m.iridescence;
  partMat.iridescenceIor = m.iridescenceIor;
  partMat.iridescenceThicknessMin = m.iridescenceThicknessMin;
  partMat.iridescenceThicknessMax = m.iridescenceThicknessMax;
  partMat.transmission = m.transmission;
  partMat.thickness = m.thickness;
  partMat.attenuationColor[0] = m.attenuationColor[0];
  partMat.attenuationColor[1] = m.attenuationColor[1];
  partMat.attenuationColor[2] = m.attenuationColor[2];
  partMat.attenuationDistance = m.attenuationDistance;
  partMat.dispersion = m.dispersion;
  uint32_t flags = 0;
  flags |= ResolveMaterialSlot(assets, m.albedoTex, 1u, partMat.albedoTex);
  flags |= ResolveMaterialSlot(assets, m.normalTex, 2u, partMat.normalTex);
  flags |= ResolveMaterialSlot(assets, m.ormTex, 4u, partMat.ormTex);
  flags |= ResolveMaterialSlot(assets, m.heightTex, 8u, partMat.heightTex);
  flags |= ResolveMaterialSlot(assets, m.emissiveTex, 32u, partMat.emissiveTex);
  flags |= ResolveMaterialSlot(assets, m.specularTex, 64u, partMat.specularTex);
  flags |= ResolveMaterialSlot(assets, m.specularColorTex, 128u, partMat.specularColorTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatTex, 256u, partMat.clearcoatTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatRoughnessTex, 512u, partMat.clearcoatRoughnessTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatNormalTex, 1024u, partMat.clearcoatNormalTex);
  flags |= ResolveMaterialSlot(assets, m.sheenColorTex, 2048u, partMat.sheenColorTex);
  flags |= ResolveMaterialSlot(assets, m.sheenRoughnessTex, 4096u, partMat.sheenRoughnessTex);
  flags |= ResolveMaterialSlot(assets, m.anisotropyTex, 8192u, partMat.anisotropyTex);
  flags |= ResolveMaterialSlot(assets, m.iridescenceTex, 16384u, partMat.iridescenceTex);
  flags |= ResolveMaterialSlot(assets, m.iridescenceThicknessTex, 32768u, partMat.iridescenceThicknessTex);
  flags |= ResolveMaterialSlot(assets, m.transmissionTex, 131072u, partMat.transmissionTex);
  flags |= ResolveMaterialSlot(assets, m.thicknessTex, 262144u, partMat.thicknessTex);
  if (m.hasSpecular) {
    flags |= 65536u;
  }
  partMat.texFlags = flags;
}
void FillDeferredFromMesh(AssetDatabase& assets, const MeshMaterialData& mm, DeferredMaterial& partMat) {
  partMat.albedo[0] = mm.albedo[0];
  partMat.albedo[1] = mm.albedo[1];
  partMat.albedo[2] = mm.albedo[2];
  partMat.alpha = mm.alpha;
  partMat.roughness = mm.roughness;
  partMat.metallic = mm.metallic;
  partMat.emissive[0] = mm.emissive[0];
  partMat.emissive[1] = mm.emissive[1];
  partMat.emissive[2] = mm.emissive[2];
  partMat.emissiveStrength = mm.emissiveStrength;
  partMat.unlit = mm.unlit;
  partMat.doubleSided = mm.doubleSided;
  partMat.alphaMode = mm.alphaMode;
  partMat.alphaCutoff = mm.alphaCutoff;
  for (int s = 0; s < 5; ++s) {
    partMat.xforms[s].offsetU = mm.xforms[s].offsetU;
    partMat.xforms[s].offsetV = mm.xforms[s].offsetV;
    partMat.xforms[s].scaleU = mm.xforms[s].scaleU;
    partMat.xforms[s].scaleV = mm.xforms[s].scaleV;
    partMat.xforms[s].rotation = mm.xforms[s].rotation;
    partMat.xforms[s].uvSet = mm.xforms[s].uvSet;
  }
  partMat.ior = mm.ior;
  partMat.hasSpecular = mm.hasSpecular;
  partMat.specularColor[0] = mm.specularColor[0];
  partMat.specularColor[1] = mm.specularColor[1];
  partMat.specularColor[2] = mm.specularColor[2];
  partMat.specularStrength = mm.specularStrength;
  partMat.clearcoat = mm.clearcoat;
  partMat.clearcoatRoughness = mm.clearcoatRoughness;
  partMat.sheenColor[0] = mm.sheenColor[0];
  partMat.sheenColor[1] = mm.sheenColor[1];
  partMat.sheenColor[2] = mm.sheenColor[2];
  partMat.sheenRoughness = mm.sheenRoughness;
  partMat.anisotropy = mm.anisotropy;
  partMat.anisotropyRotation = mm.anisotropyRotation;
  partMat.iridescence = mm.iridescence;
  partMat.iridescenceIor = mm.iridescenceIor;
  partMat.iridescenceThicknessMin = mm.iridescenceThicknessMin;
  partMat.iridescenceThicknessMax = mm.iridescenceThicknessMax;
  partMat.transmission = mm.transmission;
  partMat.thickness = mm.thickness;
  partMat.attenuationColor[0] = mm.attenuationColor[0];
  partMat.attenuationColor[1] = mm.attenuationColor[1];
  partMat.attenuationColor[2] = mm.attenuationColor[2];
  partMat.attenuationDistance = mm.attenuationDistance;
  partMat.dispersion = mm.dispersion;
  uint32_t flags = 0;
  flags |= ResolveMaterialSlot(assets, mm.albedoTexGuid, 1u, partMat.albedoTex);
  flags |= ResolveMaterialSlot(assets, mm.normalTexGuid, 2u, partMat.normalTex);
  flags |= ResolveMaterialSlot(assets, mm.ormTexGuid, 4u, partMat.ormTex);
  flags |= ResolveMaterialSlot(assets, mm.heightTexGuid, 8u, partMat.heightTex);
  flags |= ResolveMaterialSlot(assets, mm.emissiveTexGuid, 32u, partMat.emissiveTex);
  flags |= ResolveMaterialSlot(assets, mm.specularTexGuid, 64u, partMat.specularTex);
  flags |= ResolveMaterialSlot(assets, mm.specularColorTexGuid, 128u, partMat.specularColorTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatTexGuid, 256u, partMat.clearcoatTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatRoughnessTexGuid, 512u, partMat.clearcoatRoughnessTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatNormalTexGuid, 1024u, partMat.clearcoatNormalTex);
  flags |= ResolveMaterialSlot(assets, mm.sheenColorTexGuid, 2048u, partMat.sheenColorTex);
  flags |= ResolveMaterialSlot(assets, mm.sheenRoughnessTexGuid, 4096u, partMat.sheenRoughnessTex);
  flags |= ResolveMaterialSlot(assets, mm.anisotropyTexGuid, 8192u, partMat.anisotropyTex);
  flags |= ResolveMaterialSlot(assets, mm.iridescenceTexGuid, 16384u, partMat.iridescenceTex);
  flags |= ResolveMaterialSlot(assets, mm.iridescenceThicknessTexGuid, 32768u, partMat.iridescenceThicknessTex);
  flags |= ResolveMaterialSlot(assets, mm.transmissionTexGuid, 131072u, partMat.transmissionTex);
  flags |= ResolveMaterialSlot(assets, mm.thicknessTexGuid, 262144u, partMat.thicknessTex);
  if (mm.hasSpecular) {
    flags |= 65536u;
  }
  partMat.texFlags = flags;
}
static Transform s_clipboard;
static bool s_hasClipboard = false;
void EditorApp::RenderScene() {
  if (!viewValid || viewW < 8.0f || viewH < 8.0f) {
    return;
  }
  RECT rc;
  GetClientRect(static_cast<HWND>(window.NativeHandle()), &rc);
  int cw = rc.right - rc.left;
  int ch = rc.bottom - rc.top;
  if ((cw != rhi->Width() || ch != rhi->Height()) && cw > 0 && ch > 0) {
    rhi->Resize(cw, ch);
  }
  int rw = static_cast<int>(viewW);
  int rh = static_cast<int>(viewH);
  static int lastRTW = 0;
  static int lastRTH = 0;
  if (rw != lastRTW || rh != lastRTH) {
    renderer.Resize(rw, rh);
    lastRTW = rw;
    lastRTH = rh;
  }
  renderer.SetViewOffset(0.0f, 0.0f);
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  float cpx = 0.0f;
  float cpy = 0.0f;
  float cpz = 0.0f;
  camera.GetPosition(cpx, cpy, cpz);
  float cpos[3] = { cpx, cpy, cpz };
  renderer.SetFsr(viewScale, viewSharp);
  renderer.BeginObjects(&vf.m[0][0], &pf.m[0][0]);
  renderer.ClearLights();
  MainCameraSetup mainCam;
  mainCam.camPos[0] = cpx;
  mainCam.camPos[1] = cpy;
  mainCam.camPos[2] = cpz;
  mainCam.camFwd[0] = vf.m[0][2];
  mainCam.camFwd[1] = vf.m[1][2];
  mainCam.camFwd[2] = vf.m[2][2];
  mainCam.camRight[0] = vf.m[0][0];
  mainCam.camRight[1] = vf.m[1][0];
  mainCam.camRight[2] = vf.m[2][0];
  mainCam.camUp[0] = vf.m[0][1];
  mainCam.camUp[1] = vf.m[1][1];
  mainCam.camUp[2] = vf.m[2][1];
  mainCam.fovY = camera.fovY;
  mainCam.aspect = aspect;
  mainCam.nearZ = camera.nearZ;
  mainCam.farZ = camera.farZ;
  renderer.SetMainCamera(mainCam);
  renderer.SetShadowDebug(debugCascades);
  renderer.SetGrade(viewExposure, viewACES);
  renderer.SetBloom(viewBloom);
  renderer.SetSsao(viewSsao, viewSsaoIntensity, viewSsaoRadius);
  renderer.SetSsr(viewSsr);
  renderer.SetContact(viewContact);
  renderer.SetDof(viewDof, viewFocusDist, viewFocusRange);
  renderer.SetMotionBlur(viewMotion, viewMotionScale);
  renderer.SetFilm(viewCa, viewVig, viewGrain);
  renderer.SetSsgi(viewSsgi);
  renderer.SetFxaa(viewFxaa);
  renderer.SetEnvIntensity(viewEnv);
  {
    EntityId pickIds[64];
    float pickDist[64];
    int pickCount = 0;
    std::vector<EntityId> allIds = scene.All();
    for (size_t i = 0; i < allIds.size(); ++i) {
      const Entity* pe = scene.Get(allIds[i]);
      if (pe == nullptr || !pe->hasProbe) {
        continue;
      }
      float pdx = pe->transform.position[0] - cpos[0];
      float pdy = pe->transform.position[1] - cpos[1];
      float pdz = pe->transform.position[2] - cpos[2];
      float pd = pdx * pdx + pdy * pdy + pdz * pdz;
      if (pickCount < 64) {
        pickIds[pickCount] = pe->id;
        pickDist[pickCount] = pd;
        ++pickCount;
      }
    }
    RHITexture cubes[4] = { 0, 0, 0, 0 };
    float bmin[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    float bmax[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    int nprobe = 0;
    for (int k = 0; k < 4; ++k) {
      int best = -1;
      for (int i = 0; i < pickCount; ++i) {
        if (pickDist[i] < 0.0f) {
          continue;
        }
        if (best < 0 || pickDist[i] < pickDist[best]) {
          best = i;
        }
      }
      if (best < 0) {
        break;
      }
      pickDist[best] = -1.0f;
      auto cit = envProbeCubes.find(pickIds[best]);
      if (cit == envProbeCubes.end() || cit->second == 0) {
        continue;
      }
      const Entity* pe = scene.Get(pickIds[best]);
      if (pe == nullptr || !pe->hasProbe) {
        continue;
      }
      cubes[nprobe] = cit->second;
      bmin[nprobe][0] = pe->transform.position[0] - pe->probe.size[0] * 0.5f;
      bmin[nprobe][1] = pe->transform.position[1] - pe->probe.size[1] * 0.5f;
      bmin[nprobe][2] = pe->transform.position[2] - pe->probe.size[2] * 0.5f;
      bmin[nprobe][3] = pe->probe.intensity;
      bmax[nprobe][0] = pe->transform.position[0] + pe->probe.size[0] * 0.5f;
      bmax[nprobe][1] = pe->transform.position[1] + pe->probe.size[1] * 0.5f;
      bmax[nprobe][2] = pe->transform.position[2] + pe->probe.size[2] * 0.5f;
      bmax[nprobe][3] = 0.0f;
      ++nprobe;
    }
    renderer.SetEnvProbes(nprobe, cubes, bmin, bmax);
    for (auto it = envProbeCubes.begin(); it != envProbeCubes.end();) {
      if (!scene.Has(it->first)) {
        if (it->second != 0) {
          rhi->DestroyTexture(it->second);
        }
        it = envProbeCubes.erase(it);
      } else {
        ++it;
      }
    }
  }
  renderer.SetFog(viewFog, viewFogDensity, viewFogColor);
  renderer.SetEnvIntensity(viewEnv);
  SubmitSceneDraws(cpos);
  renderer.EndObjectsToTexture(cpos);
  rhi->Clear(0.03f, 0.03f, 0.04f, 1.0f);
}
void EditorApp::SubmitSceneDraws(const float cpos[3]) {
  renderer.ClearLights();
  std::vector<EntityId> lightIds = scene.All();
  for (size_t i = 0; i < lightIds.size(); ++i) {
    const Entity* e = scene.Get(lightIds[i]);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    if (e->light.type == static_cast<int>(LightType::Point)) {
      RenderPointLight pl;
      pl.pos[0] = e->transform.position[0];
      pl.pos[1] = e->transform.position[1];
      pl.pos[2] = e->transform.position[2];
      pl.color[0] = e->light.color[0];
      pl.color[1] = e->light.color[1];
      pl.color[2] = e->light.color[2];
      pl.intensity = e->light.intensity;
      pl.radius = e->light.radius;
      pl.castShadow = e->light.castShadow;
      pl.effectiveSize = e->light.softness * e->light.lightSize;
      renderer.AddPointLight(pl);
    } else if (e->light.type == static_cast<int>(LightType::Spot)) {
      RenderSpotLight sl;
      sl.pos[0] = e->transform.position[0];
      sl.pos[1] = e->transform.position[1];
      sl.pos[2] = e->transform.position[2];
      EntityForward(e->transform, sl.dir);
      sl.color[0] = e->light.color[0];
      sl.color[1] = e->light.color[1];
      sl.color[2] = e->light.color[2];
      sl.intensity = e->light.intensity;
      sl.radius = e->light.radius;
      sl.angle = e->light.spotAngle;
      sl.falloff = e->light.falloff;
      sl.castShadow = e->light.castShadow;
      sl.effectiveSize = e->light.softness * e->light.lightSize;
      sl.shadowSize = e->light.shadowSize;
      renderer.AddSpotLight(sl);
    } else if (e->light.type == static_cast<int>(LightType::Directional)) {
      RenderDirectionalLight dl;
      EntityForward(e->transform, dl.dir);
      dl.color[0] = e->light.color[0];
      dl.color[1] = e->light.color[1];
      dl.color[2] = e->light.color[2];
      dl.intensity = e->light.intensity;
      dl.castShadow = e->light.castShadow;
      dl.effectiveSize = e->light.softness * e->light.lightSize;
      dl.shadowSize = e->light.shadowSize;
      dl.cascades = e->light.cascades;
      dl.lambda = e->light.lambda;
      renderer.AddDirectionalLight(dl);
    }
  }
  DeferredMaterial grayMat;
  grayMat.albedo[0] = 0.4f;
  grayMat.albedo[1] = 0.4f;
  grayMat.albedo[2] = 0.45f;
  grayMat.roughness = 0.9f;
  grayMat.metallic = 0.0f;
  DeferredMaterial pinkMat;
  pinkMat.albedo[0] = 1.0f;
  pinkMat.albedo[1] = 0.0f;
  pinkMat.albedo[2] = 1.0f;
  pinkMat.roughness = 1.0f;
  pinkMat.metallic = 0.0f;
  std::vector<EntityId> ids = scene.All();
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    if (!e->hasMesh || e->meshGuid.empty()) {
      continue;
    }
    std::string drawGuids[2];
    float drawFades[2] = { 0.0f, 0.0f };
    int drawCount = 1;
    drawGuids[0] = e->meshGuid;
    drawGuids[1] = "";
    if (e->hasLod && !e->lod.farGuid.empty() && e->lod.farGuid != e->meshGuid && e->lod.farDist > e->lod.nearDist) {
      float ldx = e->transform.position[0] - cpos[0];
      float ldy = e->transform.position[1] - cpos[1];
      float ldz = e->transform.position[2] - cpos[2];
      float ldist = sqrtf(ldx * ldx + ldy * ldy + ldz * ldz);
      float lt = (ldist - e->lod.nearDist) / (e->lod.farDist - e->lod.nearDist);
      if (lt >= 1.0f) {
        drawGuids[0] = e->lod.farGuid;
      } else if (lt > 0.0f) {
        drawGuids[1] = e->lod.farGuid;
        drawFades[0] = 1.0f - lt;
        drawFades[1] = lt;
        drawCount = 2;
      }
    }
    bool anyDrawn = false;
    for (int li = 0; li < drawCount; ++li) {
      const MeshRecord* rec = assets.GetByGuid(drawGuids[li]);
      if (rec != nullptr && rec->loaded && assets.EnsureMeshGpu(drawGuids[li], 4194304)) {
        rec = assets.GetByGuid(drawGuids[li]);
          if (rec != nullptr && rec->gpuReady) {
            if (rec->data.parts.empty()) {
              renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, 0, rec->gpuCount, nullptr, drawFades[li], rec->data.aabbMin, rec->data.aabbMax);
          } else {
          for (size_t p = 0; p < rec->data.parts.size(); ++p) {
            const MeshPartData& part = rec->data.parts[p];
            const DeferredMaterial* useMat = nullptr;
            DeferredMaterial partMat;
            const MaterialData* pm = nullptr;
            for (size_t k = 0; k < e->partMaterials.size(); ++k) {
              if (e->partMaterials[k].part == static_cast<uint32_t>(p)) {
                pm = &e->partMaterials[k].material;
                break;
              }
            }
            if (pm != nullptr) {
              FillDeferredFromEntity(assets, *pm, partMat);
              useMat = &partMat;
            } else if (e->hasMaterial) {
              FillDeferredFromEntity(assets, e->material, partMat);
              useMat = &partMat;
            } else {
              useMat = nullptr;
            }
            if (useMat == nullptr && part.material < rec->data.materials.size()) {
              uint32_t matIdx = part.material;
              if (e->variantIndex >= 0) {
                for (size_t vi = 0; vi < rec->data.variantAssigns.size(); ++vi) {
                  const MeshVariantAssign& va = rec->data.variantAssigns[vi];
                  if (va.part == static_cast<uint32_t>(p) && static_cast<int>(va.variant) == e->variantIndex && va.material < rec->data.materials.size()) {
                    matIdx = va.material;
                    break;
                  }
                }
              }
              const MeshMaterialData& mm = rec->data.materials[matIdx];
              FillDeferredFromMesh(assets, mm, partMat);
              useMat = &partMat;
            }
            if (e->hasLightmap && !e->lightmap.texGuid.empty() && useMat != nullptr) {
              RHITexture lmTex = 0;
              if (ResolveMaterialSlot(assets, e->lightmap.texGuid, 16u, lmTex) != 0) {
                partMat.lightmapTex = lmTex;
                partMat.lightmapIntensity = e->lightmap.intensity;
                partMat.texFlags |= 16u;
              }
            }
            renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, part.indexOffset, part.indexCount, useMat, drawFades[li], rec->data.aabbMin, rec->data.aabbMax);
          }
        }
        anyDrawn = true;
      }
    }
    }
    if (!anyDrawn && defaultVB != 0) {
      const MeshRecord* baserec = assets.GetByGuid(e->meshGuid);
      const DeferredMaterial* fallback = &grayMat;
      if (baserec == nullptr) {
        fallback = &pinkMat;
      }
      renderer.DrawObjectEx(world, defaultVB, defaultIB, 0, defaultCount, fallback);
    }
  }
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr || !e->hasDecal || e->decal.texGuid.empty()) {
      continue;
    }
    float age = 0.0f;
    auto ag = decalAge.find(e->id);
    if (ag != decalAge.end()) {
      age = ag->second;
    }
    age += frameDt;
    decalAge[e->id] = age;
    float fade = 1.0f;
    if (e->decal.life > 0.0f) {
      if (age >= e->decal.life) {
        continue;
      }
      fade = 1.0f - age / e->decal.life;
    }
    RHITexture dtex = 0;
    ResolveMaterialSlot(assets, e->decal.texGuid, 1u, dtex);
    if (dtex == 0) {
      continue;
    }
    Transform dt = e->transform;
    dt.scale[0] *= e->decal.size[0];
    dt.scale[1] *= e->decal.size[1];
    dt.scale[2] *= e->decal.size[2];
    float dworld[16];
    ComposeMatrix(dt, dworld);
    float dinv[16];
    if (!InvertMatrix4(dworld, dinv)) {
      continue;
    }
    renderer.DrawDecal(dworld, dinv, e->decal.size, fade, dtex);
  }
  for (auto it = decalAge.begin(); it != decalAge.end();) {
    if (!scene.Has(it->first)) {
      it = decalAge.erase(it);
    } else {
      ++it;
    }
  }
}
void EditorApp::BakeEnvironment() {
  float px = 0.0f;
  float py = 0.0f;
  float pz = 0.0f;
  camera.GetPosition(px, py, pz);
  if (!renderer.BeginEnvCapture(128)) {
    Announce(LogLevel::Error, "Environment capture failed");
    return;
  }
  BakeFaces(px, py, pz);
  renderer.GenerateEnvMips();
  Announce(LogLevel::Success, "Environment baked");
}
void EditorApp::BakeFaces(float px, float py, float pz) {
  float dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
  float ups[6][3] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
  DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(1.5707963f, 1.0f, 0.1f, 200.0f);
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&pf, proj);
  float eye[3] = { px, py, pz };
  for (int f = 0; f < 6; ++f) {
    DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(px, py, pz, 1.0f);
    DirectX::XMVECTOR atV = DirectX::XMVectorSet(px + dirs[f][0], py + dirs[f][1], pz + dirs[f][2], 1.0f);
    DirectX::XMVECTOR upV = DirectX::XMVectorSet(ups[f][0], ups[f][1], ups[f][2], 0.0f);
    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eyeV, atV, upV);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMStoreFloat4x4(&vf, view);
    MainCameraSetup mainCam;
    mainCam.camPos[0] = px;
    mainCam.camPos[1] = py;
    mainCam.camPos[2] = pz;
    mainCam.camFwd[0] = vf.m[2][0];
    mainCam.camFwd[1] = vf.m[2][1];
    mainCam.camFwd[2] = vf.m[2][2];
    mainCam.camRight[0] = vf.m[0][0];
    mainCam.camRight[1] = vf.m[0][1];
    mainCam.camRight[2] = vf.m[0][2];
    mainCam.camUp[0] = vf.m[1][0];
    mainCam.camUp[1] = vf.m[1][1];
    mainCam.camUp[2] = vf.m[1][2];
    mainCam.fovY = 1.5707963f;
    mainCam.aspect = 1.0f;
    mainCam.nearZ = 0.1f;
    mainCam.farZ = 200.0f;
    renderer.SetMainCamera(mainCam);
    renderer.BeginEnvFace(f, &vf.m[0][0], &pf.m[0][0]);
    SubmitSceneDraws(eye);
    renderer.EndEnvFace(eye);
  }
}
void EditorApp::BakeProbeEntity(EntityId id) {
  Entity* e = scene.Get(id);
  if (e == nullptr || !e->hasProbe) {
    return;
  }
  RHITexture cube = rhi->CreateEnvCube(64);
  if (cube == 0) {
    Announce(LogLevel::Error, "Probe cube failed");
    return;
  }
  if (!renderer.BeginProbeCapture(cube)) {
    rhi->DestroyTexture(cube);
    Announce(LogLevel::Error, "Probe capture failed");
    return;
  }
  BakeFaces(e->transform.position[0], e->transform.position[1], e->transform.position[2]);
  renderer.GenerateEnvMips();
  auto it = envProbeCubes.find(id);
  if (it != envProbeCubes.end() && it->second != 0) {
    rhi->DestroyTexture(it->second);
  }
  envProbeCubes[id] = cube;
  log.Add(LogLevel::Info, std::string("Probe baked: ") + e->name);
}
void EditorApp::BakeProbes() {
  int done = 0;
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e != nullptr && e->hasProbe) {
      BakeProbeEntity(all[i]);
      ++done;
    }
  }
  if (done == 0) {
    Announce(LogLevel::Warning, "No probes in scene");
  } else {
    Announce(LogLevel::Success, std::string("Baked ") + std::to_string(done) + " probes");
  }
}
void EditorApp::BakeLightmap(EntityId id) {
  std::string path;
  std::string guid;
  std::string err;
  if (!BakeLightmapFor(scene, assets, id, 128, path, guid, err)) {
    Announce(LogLevel::Error, std::string("Lightmap bake failed: ") + err);
    return;
  }
  assets.Scan();
  Entity* e = scene.Get(id);
  if (e == nullptr || !e->hasLightmap) {
    Announce(LogLevel::Warning, "Baked, but entity lost Lightmap component");
    return;
  }
  LightmapData after = e->lightmap;
  after.texGuid = guid;
  std::unique_ptr<Command> cmd(new SetLightmapCmd(id, e->lightmap, after));
  if (undo.Execute(std::move(cmd), scene)) {
    log.Add(LogLevel::Info, std::string("Lightmap baked: ") + path);
  }
}
void EditorApp::HandleViewportClick() {
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  ImVec2 mp = ImGui::GetMousePos();
  float origin[3];
  float dir[3];
  ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
  EntityId hit = PickFirst(scene, origin, dir);
  ImGuiIO& cio = ImGui::GetIO();
  if (hit.IsValid()) {
    if (cio.KeyShift) {
      selection.Add(hit);
    } else if (cio.KeyCtrl) {
      selection.Toggle(hit);
    } else {
      selection.Select(hit);
    }
  } else if (!cio.KeyShift && !cio.KeyCtrl) {
    selection.Clear();
  }
}
void EditorApp::HandleRubberSelect(float x0, float y0, float x1, float y1) {
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  std::vector<EntityId> hits;
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    float ex0;
    float ey0;
    float ex1;
    float ey1;
    if (EntityScreenRect(scene, all[i], &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, ex0, ey0, ex1, ey1)) {
      if (RectsOverlap(x0, y0, x1, y1, ex0, ey0, ex1, ey1)) {
        hits.push_back(all[i]);
      }
    }
  }
  ImGuiIO& rio = ImGui::GetIO();
  if (rio.KeyShift) {
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Add(hits[i]);
    }
  } else if (rio.KeyCtrl) {
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Toggle(hits[i]);
    }
  } else {
    selection.Clear();
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Add(hits[i]);
    }
  }
  log.Add(LogLevel::Info, std::string("Rubber select: ") + std::to_string(hits.size()));
}
void EditorApp::DrawViewport() {
  ImGui::Begin("Viewport", &showViewport);
  ImGuizmo::OPERATION curOp = static_cast<ImGuizmo::OPERATION>(gizmoOp);
  if (ImGui::RadioButton("Move", curOp == ImGuizmo::TRANSLATE)) {
    gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Rotate", curOp == ImGuizmo::ROTATE)) {
    gizmoOp = static_cast<int>(ImGuizmo::ROTATE);
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Scale", curOp == ImGuizmo::SCALE)) {
    gizmoOp = static_cast<int>(ImGuizmo::SCALE);
  }
  if (ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput && !RawInputPoll::IsMouseDown(VK_RBUTTON)) {
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::ROTATE);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::SCALE);
    }
  }
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float iw = avail.x > 8.0f ? avail.x : 8.0f;
  float ih = avail.y > 8.0f ? avail.y : 8.0f;
  ImVec2 ipos = ImGui::GetCursorScreenPos();
  viewX = ipos.x;
  viewY = ipos.y;
  viewW = iw;
  viewH = ih;
  viewValid = true;
  ImDrawList* vdl = ImGui::GetWindowDrawList();
  vdl->ChannelsSplit(2);
  vdl->ChannelsSetCurrent(1);
  if (selection.HasSelection()) {
    SyncSelection();
  }
  if (selection.HasSelection()) {
    DirectX::XMMATRIX view = camera.View();
    float aspect = viewW / viewH;
    DirectX::XMMATRIX proj = camera.Projection(aspect);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, view);
    DirectX::XMStoreFloat4x4(&pf, proj);
    if (selection.Count() == 1) {
      Entity* e = scene.Get(selection.Get());
      if (e == nullptr) {
        selection.Clear();
        gizmoDragging = false;
      } else {
        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(e->transform.position, e->transform.rotation, e->transform.scale, matrix);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewX, viewY, viewW, viewH);
        if (ImGuizmo::Manipulate(&vf.m[0][0], &pf.m[0][0], static_cast<ImGuizmo::OPERATION>(gizmoOp), ImGuizmo::WORLD, matrix)) {
          if (!gizmoDragging) {
            gizmoDragging = true;
            gizmoStart = e->transform;
            gizmoTarget = e->id;
          }
          Transform t = e->transform;
          ImGuizmo::DecomposeMatrixToComponents(matrix, t.position, t.rotation, t.scale);
          scene.SetTransform(e->id, t);
        } else if (gizmoDragging) {
          gizmoDragging = false;
          gizmoJustEnded = true;
          const Entity* cur = scene.Get(gizmoTarget);
          if (cur != nullptr && std::memcmp(&gizmoStart, &cur->transform, sizeof(Transform)) != 0) {
            std::unique_ptr<Command> cmd(new EditTransformCmd(gizmoTarget, gizmoStart, cur->transform));
            undo.Commit(std::move(cmd));
          }
          gizmoTarget = EntityId::Invalid();
        }
      }
    } else {
      float avg[3] = { 0.0f, 0.0f, 0.0f };
      std::vector<EntityId> members = selection.All();
      size_t alive = 0;
      for (size_t i = 0; i < members.size(); ++i) {
        const Entity* e = scene.Get(members[i]);
        if (e == nullptr) {
          continue;
        }
        avg[0] += e->transform.position[0];
        avg[1] += e->transform.position[1];
        avg[2] += e->transform.position[2];
        ++alive;
      }
      if (alive == 0) {
        selection.Clear();
        gizmoDragging = false;
      } else {
        avg[0] /= static_cast<float>(alive);
        avg[1] /= static_cast<float>(alive);
        avg[2] /= static_cast<float>(alive);
        float one[3] = { 1.0f, 1.0f, 1.0f };
        float zero[3] = { 0.0f, 0.0f, 0.0f };
        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(avg, zero, one, matrix);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewX, viewY, viewW, viewH);
        if (ImGuizmo::Manipulate(&vf.m[0][0], &pf.m[0][0], static_cast<ImGuizmo::OPERATION>(gizmoOp), ImGuizmo::WORLD, matrix)) {
          if (!gizmoDragging) {
            gizmoDragging = true;
            gizmoOrigins.clear();
            for (size_t i = 0; i < members.size(); ++i) {
              const Entity* e = scene.Get(members[i]);
              if (e != nullptr) {
                gizmoOrigins[members[i]] = e->transform;
              }
            }
          }
          float npos[3];
          float nrot[3];
          float nscl[3];
          ImGuizmo::DecomposeMatrixToComponents(matrix, npos, nrot, nscl);
          float dpos[3] = { npos[0] - avg[0], npos[1] - avg[1], npos[2] - avg[2] };
          float dfac[3] = { nscl[0] < 0.01f ? 0.01f : nscl[0], nscl[1] < 0.01f ? 0.01f : nscl[1], nscl[2] < 0.01f ? 0.01f : nscl[2] };
          for (auto& kv : gizmoOrigins) {
            Entity* e = scene.Get(kv.first);
            if (e == nullptr) {
              continue;
            }
            Transform t = kv.second;
            t.position[0] += dpos[0];
            t.position[1] += dpos[1];
            t.position[2] += dpos[2];
            t.rotation[0] += nrot[0];
            t.rotation[1] += nrot[1];
            t.rotation[2] += nrot[2];
            t.scale[0] *= dfac[0];
            t.scale[1] *= dfac[1];
            t.scale[2] *= dfac[2];
            if (t.scale[0] < 0.01f) {
              t.scale[0] = 0.01f;
            }
            if (t.scale[1] < 0.01f) {
              t.scale[1] = 0.01f;
            }
            if (t.scale[2] < 0.01f) {
              t.scale[2] = 0.01f;
            }
            scene.SetTransform(e->id, t);
          }
        } else if (gizmoDragging) {
          gizmoDragging = false;
          gizmoJustEnded = true;
          MultiEditTransformCmd* multi = new MultiEditTransformCmd();
          for (auto& kv : gizmoOrigins) {
            const Entity* cur = scene.Get(kv.first);
            if (cur != nullptr && std::memcmp(&kv.second, &cur->transform, sizeof(Transform)) != 0) {
              multi->Add(kv.first, kv.second, cur->transform);
            }
          }
          if (!multi->Empty()) {
            std::unique_ptr<Command> cmd(multi);
            undo.Commit(std::move(cmd));
          } else {
            delete multi;
          }
          gizmoOrigins.clear();
        }
      }
    }
  } else {
    gizmoDragging = false;
    gizmoOrigins.clear();
  }
  vdl->ChannelsSetCurrent(0);
  gizmoHotLast = ImGuizmo::IsOver();
  void* tex = renderer.IsReady() ? renderer.GetViewportTexture() : nullptr;
  if (tex != nullptr) {
    ImGui::Image(reinterpret_cast<ImTextureID>(tex), ImVec2(iw, ih));
  } else {
    ImGui::Dummy(ImVec2(iw, ih));
  }
  bool showButton = !gizmoHotLast;
  bool canvasHovered = false;
  if (showButton) {
    ImGui::SetCursorScreenPos(ipos);
    ImGui::InvisibleButton("ViewCanvas", ImVec2(iw, ih));
    canvasHovered = ImGui::IsItemHovered();
  }
  bool lookNow = canvasHovered && RawInputPoll::IsMouseDown(VK_RBUTTON);
  UpdateCamera(frameDt, lookNow);
  if (showButton && ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ASSET");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      const MeshRecord* rec = assets.GetByGuid(guid);
      if (rec == nullptr) {
        Announce(LogLevel::Warning, "Dropped asset not found");
      } else {
        DirectX::XMMATRIX view = camera.View();
        float aspect = viewW / viewH;
        DirectX::XMMATRIX proj = camera.Projection(aspect);
        DirectX::XMFLOAT4X4 vf;
        DirectX::XMFLOAT4X4 pf;
        DirectX::XMStoreFloat4x4(&vf, view);
        DirectX::XMStoreFloat4x4(&pf, proj);
        ImVec2 mp = ImGui::GetMousePos();
        float origin[3];
        float dir[3];
        ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
        float t = -1.0f;
        if (fabsf(dir[1]) > 1e-6f) {
          t = -origin[1] / dir[1];
        }
        if (t > 0.0f) {
          CreateEntityAt(origin[0] + dir[0] * t, 0.0f, origin[2] + dir[2] * t, guid);
        } else {
          CreateEntityAt(origin[0] + dir[0] * 5.0f, origin[1] + dir[1] * 5.0f, origin[2] + dir[2] * 5.0f, guid);
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    downX = static_cast<int>(ImGui::GetMousePos().x);
    downY = static_cast<int>(ImGui::GetMousePos().y);
    downPosValid = true;
  }
  if (downPosValid && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (gizmoJustEnded) {
      gizmoJustEnded = false;
    } else {
      int ux = static_cast<int>(ImGui::GetMousePos().x);
      int uy = static_cast<int>(ImGui::GetMousePos().y);
      int dx = ux - downX;
      int dy = uy - downY;
      if (dx * dx + dy * dy < 25 && canvasHovered) {
        HandleViewportClick();
      } else if (canvasHovered) {
        HandleRubberSelect(static_cast<float>(downX), static_cast<float>(downY), static_cast<float>(ux), static_cast<float>(uy));
      }
    }
    downPosValid = false;
    rubberActive = false;
  }
  if (downPosValid && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImVec2 mp = ImGui::GetMousePos();
    float dx = mp.x - static_cast<float>(downX);
    float dy = mp.y - static_cast<float>(downY);
    if (dx * dx + dy * dy >= 25.0f) {
      rubberActive = true;
      rubberX0 = static_cast<float>(downX);
      rubberY0 = static_cast<float>(downY);
      ImGui::GetWindowDrawList()->AddRect(ImVec2(rubberX0, rubberY0), mp, IM_COL32(255, 255, 255, 255));
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(rubberX0, rubberY0), mp, IM_COL32(120, 180, 255, 40));
    } else {
      rubberActive = false;
    }
  }
  if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    rdownX = static_cast<int>(ImGui::GetMousePos().x);
    rdownY = static_cast<int>(ImGui::GetMousePos().y);
    rdownValid = true;
  }
  if (rdownValid && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    int ux = static_cast<int>(ImGui::GetMousePos().x);
    int uy = static_cast<int>(ImGui::GetMousePos().y);
    int rdx = ux - rdownX;
    int rdy = uy - rdownY;
    rdownValid = false;
    if (rdx * rdx + rdy * rdy < 25 && canvasHovered) {
      DirectX::XMMATRIX view = camera.View();
      float aspect = viewW / viewH;
      DirectX::XMMATRIX proj = camera.Projection(aspect);
      DirectX::XMFLOAT4X4 vf;
      DirectX::XMFLOAT4X4 pf;
      DirectX::XMStoreFloat4x4(&vf, view);
      DirectX::XMStoreFloat4x4(&pf, proj);
      ImVec2 mp = ImGui::GetMousePos();
      float origin[3];
      float dir[3];
      ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
      contextPick = PickFirst(scene, origin, dir);
      if (contextPick.IsValid()) {
        selection.Select(contextPick);
      }
      ImGui::OpenPopup("ViewportContext");
    }
  }
  if (ImGui::BeginPopup("ViewportContext")) {
    if (contextPick.IsValid() || selection.HasSelection()) {
      EntityId target = contextPick.IsValid() ? contextPick : selection.Get();
      const Entity* e = scene.Get(target);
      if (e != nullptr) {
        ImGui::Text("%s", e->name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Focus")) {
          FocusEntity(target);
        }
        if (ImGui::MenuItem("Duplicate")) {
          DuplicateViaCommand(target);
        }
        if (ImGui::MenuItem("Delete")) {
          DeleteViaCommand(target);
        }
      }
    } else {
      if (ImGui::MenuItem("Create Entity Here")) {
        DirectX::XMMATRIX view = camera.View();
        float aspect = viewW / viewH;
        DirectX::XMMATRIX proj = camera.Projection(aspect);
        DirectX::XMFLOAT4X4 vf;
        DirectX::XMFLOAT4X4 pf;
        DirectX::XMStoreFloat4x4(&vf, view);
        DirectX::XMStoreFloat4x4(&pf, proj);
        ImVec2 mp = ImGui::GetMousePos();
        float origin[3];
        float dir[3];
        ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
        float t = -1.0f;
        if (fabsf(dir[1]) > 1e-6f) {
          t = -origin[1] / dir[1];
        }
        if (t > 0.0f) {
          CreateEntityAt(origin[0] + dir[0] * t, 0.0f, origin[2] + dir[2] * t);
        } else {
          CreateEntityAt(0.0f, 0.0f, 0.0f);
        }
      }
    }
    ImGui::EndPopup();
  }
  DrawLightGizmo();
  vdl->ChannelsMerge();
  ImGui::End();
}
void EditorApp::DrawLightGizmo() {
  if (!selection.HasSelection()) {
    return;
  }
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  std::vector<EntityId> sel = selection.All();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->ChannelsSetCurrent(1);
  float pts[144][3];
  for (size_t i = 0; i < sel.size(); ++i) {
    const Entity* e = scene.Get(sel[i]);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    float pos[3] = { e->transform.position[0], e->transform.position[1], e->transform.position[2] };
    float dir[3] = { 0.0f, 0.0f, 1.0f };
    EntityForward(e->transform, dir);
    int segs = 0;
    if (e->light.type == static_cast<int>(LightType::Point)) {
      LightSpherePoints(pos, e->light.radius, pts);
      segs = LightSphereSegs;
    } else if (e->light.type == static_cast<int>(LightType::Spot)) {
      LightConePoints(pos, dir, e->light.spotAngle, e->light.radius, pts);
      segs = LightConeSegs;
    } else if (e->light.type == static_cast<int>(LightType::Directional)) {
      LightArrowPoints(pos, dir, 3.0f, pts);
      segs = LightArrowSegs;
    } else {
      continue;
    }
    for (int s = 0; s < segs; ++s) {
      float ax = 0.0f;
      float ay = 0.0f;
      float bx = 0.0f;
      float by = 0.0f;
      if (ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], pts[s * 2], viewX, viewY, viewW, viewH, ax, ay) &&
          ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], pts[s * 2 + 1], viewX, viewY, viewW, viewH, bx, by)) {
        dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), IM_COL32(255, 210, 60, 255), 1.5f);
      }
    }
  }
}
void EditorApp::DrawSettings() {
  ImGui::Begin("Settings", &showSettings);
  if (ImGui::BeginTabBar("SettingsTabs")) {
    if (ImGui::BeginTabItem("Post")) {
      ImGui::SliderFloat("Exposure", &viewExposure, 0.25f, 4.0f);
      ImGui::Checkbox("ACES", &viewACES);
      ImGui::SliderFloat("Bloom", &viewBloom, 0.0f, 2.0f);
      ImGui::Separator();
      ImGui::Checkbox("SSAO", &viewSsao);
      ImGui::SliderFloat("SSAO Intensity", &viewSsaoIntensity, 0.0f, 2.0f);
      ImGui::SliderFloat("SSAO Radius", &viewSsaoRadius, 0.1f, 2.0f);
      ImGui::Separator();
      ImGui::Checkbox("SSR", &viewSsr);
      ImGui::Checkbox("Contact Shadows", &viewContact);
      ImGui::Checkbox("SSGI", &viewSsgi);
      ImGui::Separator();
      ImGui::Checkbox("DoF", &viewDof);
      ImGui::SliderFloat("Focus Dist", &viewFocusDist, 1.0f, 100.0f);
      ImGui::SliderFloat("Focus Range", &viewFocusRange, 0.5f, 60.0f);
      ImGui::Checkbox("Motion Blur", &viewMotion);
      ImGui::SliderFloat("Motion Scale", &viewMotionScale, 0.0f, 4.0f);
      ImGui::SliderFloat("Chromatic Aberration", &viewCa, 0.0f, 2.0f);
      ImGui::SliderFloat("Vignette", &viewVig, 0.0f, 1.0f);
      ImGui::SliderFloat("Film Grain", &viewGrain, 0.0f, 0.25f);
      ImGui::Separator();
      ImGui::Checkbox("Fog", &viewFog);
      ImGui::SliderFloat("Fog Density", &viewFogDensity, 0.0f, 0.05f);
      ImGui::ColorEdit3("Fog Color", viewFogColor);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Render")) {
      int q = viewQuality < 0 ? 2 : (viewQuality > 3 ? 3 : viewQuality);
      if (ImGui::Combo("Quality", &q, "Low\0Medium\0High\0Ultra\0")) {
        viewQuality = q;
        Kizuri::QualitySettings qs = Kizuri::ApplyQualityPreset(static_cast<Kizuri::QualityLevel>(q));
        viewSsao = qs.ssao;
        viewSsr = qs.ssr;
        viewContact = qs.contact;
        viewSsgi = qs.ssgi;
        viewDof = qs.dof;
        viewMotion = qs.motionBlur;
        viewFxaa = qs.fxaa;
        viewFog = qs.fog;
        viewBloom = qs.bloom;
        viewScale = qs.renderScale;
      }
      ImGui::SliderFloat("Render Scale", &viewScale, 0.5f, 1.0f);
      ImGui::SliderFloat("Sharpness", &viewSharp, 0.0f, 1.0f);
      if (ImGui::Checkbox("Dynamic Resolution", &viewDynRes)) {
        renderer.SetDynamicResolution(viewDynRes, viewDynTarget, 0.5f, 1.0f);
      }
      if (viewDynRes) {
        if (ImGui::SliderFloat("Target ms", &viewDynTarget, 8.0f, 33.0f)) {
          renderer.SetDynamicResolution(true, viewDynTarget, 0.5f, 1.0f);
        }
        viewScale = renderer.GetDynamicScale();
      }
      ImGui::Checkbox("FXAA", &viewFxaa);
      ImGui::SliderFloat("Env Intensity", &viewEnv, 0.0f, 4.0f);
      if (ImGui::Button("Bake Environment")) {
        BakeEnvironment();
      }
      if (ImGui::Button("Bake Probes")) {
        BakeProbes();
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Debug")) {
      ImGui::Checkbox("Debug CSM", &debugCascades);
      if (ImGui::Checkbox("Occlusion Culling", &viewOcc)) {
        renderer.SetOcclusionCulling(viewOcc);
      }
      const Kizuri::RenderStats& rst = renderer.GetStats();
      ImGui::Text("Draws geo %u shadow %u fwd %u decal %u", rst.geometryDraws, rst.shadowDraws, rst.forwardDraws, rst.decalDraws);
      ImGui::Text("Tris geo %llu shadow %llu fwd %llu", (unsigned long long)rst.geometryTris, (unsigned long long)rst.shadowTris, (unsigned long long)rst.forwardTris);
      ImGui::Text("Culled %u occluded %u post %u", rst.culledDraws, rst.occludedDraws, rst.postPasses);
      ImGui::Text("CPU total %.2f submit %.2f shadow %.2f light %.2f", rst.msTotal, rst.msSubmit, rst.msShadow, rst.msLighting);
      if (rst.gpuValid) {
        ImGui::Text("GPU total %.2f shadow %.2f light %.2f post %.2f", rst.gpuMs, rst.gpuShadowMs, rst.gpuLightMs, rst.gpuPostMs);
      } else {
        ImGui::Text("GPU n/a");
      }
      ImGui::Text("Mem tex %.1fMB targets %.1fMB bufs %.1fMB", rst.textureBytes / 1048576.0, rst.targetBytes / 1048576.0, rst.bufferBytes / 1048576.0);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}
void EditorApp::DrawHierarchy() {
  ImGui::Begin("Hierarchy", &showHierarchy);
  if (ImGui::Button("Create Entity")) {
    CreateEntityAt(0.0f, 0.0f, 0.0f);
  }
  ImGui::Separator();
  std::vector<EntityId> all = scene.All();
  bool itemMenu = false;
  for (size_t i = 0; i < all.size(); ++i) {
    Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    ImGui::PushID(static_cast<int>(e->id.index * 1315423911u + e->id.generation));
    bool isSel = selection.Contains(e->id);
    if (renameActive && renameTarget == e->id) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##rename", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string oldName = e->name;
        std::string newName = renameBuf;
        if (oldName != newName) {
          std::unique_ptr<Command> cmd(new RenameCmd(e->id, oldName, newName));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, std::string("Renamed to ") + newName);
          }
        }
        renameActive = false;
      }
      if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        renameActive = false;
      }
    } else {
      if (ImGui::Selectable(e->name.c_str(), isSel)) {
        ImGuiIO& hio = ImGui::GetIO();
        if (hio.KeyShift) {
          selection.Add(e->id);
        } else if (hio.KeyCtrl) {
          selection.Toggle(e->id);
        } else {
          selection.Select(e->id);
        }
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        renameActive = true;
        renameTarget = e->id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
    }
    if (ImGui::BeginPopupContextItem("EntityContext")) {
      itemMenu = true;
      if (ImGui::MenuItem("Create Entity")) {
        CreateEntityAt(0.0f, 0.0f, 0.0f);
      }
      if (ImGui::MenuItem("Duplicate")) {
        DuplicateViaCommand(e->id);
      }
      if (ImGui::MenuItem("Delete")) {
        DeleteViaCommand(e->id);
      }
      if (ImGui::MenuItem("Rename")) {
        renameActive = true;
        renameTarget = e->id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Focus")) {
        FocusEntity(e->id);
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
  if (!itemMenu && ImGui::BeginPopupContextWindow("HierarchyContext")) {
    if (ImGui::MenuItem("Create Entity")) {
      CreateEntityAt(0.0f, 0.0f, 0.0f);
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
void EditorApp::DrawInspector() {
  ImGui::Begin("Inspector", &showInspector);
  if (!selection.HasSelection()) {
    ImGui::Text("No selection");
    ImGui::End();
    return;
  }
  Entity* e = scene.Get(selection.Get());
  if (e == nullptr) {
    selection.Clear();
    ImGui::Text("No selection");
    ImGui::End();
    return;
  }
  ImGui::Text("Entity: %s", e->name.c_str());
  if (selection.Count() > 1) {
    ImGui::Text("%llu selected", (unsigned long long)selection.Count());
  }
  ImGui::Separator();
  ImGui::Text("Transform");
  Transform t = e->transform;
  bool changed = false;
  if (ImGui::DragFloat3("Position", t.position, 0.05f)) {
    changed = true;
  }
  if (ImGui::DragFloat3("Rotation", t.rotation, 0.5f)) {
    changed = true;
  }
  if (ImGui::DragFloat3("Scale", t.scale, 0.02f)) {
    if (t.scale[0] < 0.01f) {
      t.scale[0] = 0.01f;
    }
    if (t.scale[1] < 0.01f) {
      t.scale[1] = 0.01f;
    }
    if (t.scale[2] < 0.01f) {
      t.scale[2] = 0.01f;
    }
    changed = true;
  }
  if (changed) {
    edits.PushTransform(e->id, t);
  }
  ImGui::Separator();
  if (ImGui::Button("Reset Transform")) {
    Transform ident;
    MakeIdentityTransform(ident);
    edits.PushTransform(e->id, ident);
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy")) {
    s_clipboard = e->transform;
    s_hasClipboard = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste")) {
    if (s_hasClipboard) {
      edits.PushTransform(e->id, s_clipboard);
    } else {
      Announce(LogLevel::Warning, "Clipboard empty");
    }
  }
  if (e->hasMesh) {
    ImGui::Separator();
    ImGui::Text("Mesh Renderer");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##mesh")) {
      std::unique_ptr<Command> cmd(new RemoveMeshCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    DrawMeshSection(e);
  }
  if (e->hasLight) {
    ImGui::Separator();
    ImGui::Text("Light");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##light")) {
      std::unique_ptr<Command> cmd(new RemoveLightCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    LightData light = e->light;
    int lightMode = light.type;
    if (lightMode < 0 || lightMode > 2) {
      lightMode = 0;
    }
    bool lightChanged = false;
    if (ImGui::Combo("Type", &lightMode, "Point\0Spot\0Directional\0")) {
      light.type = lightMode;
      lightChanged = true;
    }
    if (ImGui::ColorEdit3("Color", light.color)) {
      lightChanged = true;
    }
    if (ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 20.0f)) {
      lightChanged = true;
    }
    if (light.type == static_cast<int>(LightType::Point) || light.type == static_cast<int>(LightType::Spot)) {
      if (ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.5f, 100.0f)) {
        lightChanged = true;
      }
    }
    if (light.type == static_cast<int>(LightType::Spot)) {
      if (ImGui::DragFloat("Angle", &light.spotAngle, 0.5f, 1.0f, 170.0f)) {
        lightChanged = true;
      }
      if (ImGui::DragFloat("Falloff", &light.falloff, 0.01f, 0.01f, 0.5f)) {
        lightChanged = true;
      }
    }
    bool showShadow = light.castShadow;
    if (ImGui::Checkbox("Cast Shadow", &showShadow)) {
      light.castShadow = showShadow;
      lightChanged = true;
    }
    if (light.castShadow) {
      if (ImGui::DragFloat("Light Size", &light.lightSize, 0.01f, 0.0f, 2.0f)) {
        lightChanged = true;
      }
      int resMode = 1;
      if (light.shadowSize == 512) {
        resMode = 0;
      } else if (light.shadowSize == 2048) {
        resMode = 2;
      }
      if (ImGui::Combo("Shadow Size", &resMode, "512\0""1024\0""2048\0")) {
        light.shadowSize = resMode == 0 ? 512 : (resMode == 2 ? 2048 : 1024);
        lightChanged = true;
      }
      if (ImGui::DragFloat("Softness", &light.softness, 0.01f, 0.0f, 2.0f)) {
        lightChanged = true;
      }
      if (light.type == static_cast<int>(LightType::Directional)) {
        if (ImGui::SliderInt("Cascades", &light.cascades, 2, 4)) {
          lightChanged = true;
        }
        if (ImGui::DragFloat("Lambda", &light.lambda, 0.01f, 0.0f, 1.0f)) {
          lightChanged = true;
        }
      }
    }
    if (lightChanged) {
      edits.PushLight(e->id, light);
    }
  }
  if (e->hasMaterial) {
    ImGui::Separator();
    ImGui::Text("Material");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##material")) {
      std::unique_ptr<Command> cmd(new RemoveMaterialCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    MaterialData mat = e->material;
    if (DrawMaterialScalars(mat)) {
      edits.PushMaterial(e->id, mat);
    }
    DrawMaterialSection(e);
  }
  if (e->hasDecal) {
    ImGui::Separator();
    ImGui::Text("Decal");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##decal")) {
      std::unique_ptr<Command> cmd(new RemoveDecalCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    DecalData dec = e->decal;
    bool decChanged = false;
    if (ImGui::DragFloat3("Decal Size", dec.size, 0.05f, 0.05f, 50.0f)) {
      decChanged = true;
    }
    if (ImGui::DragFloat("Life", &dec.life, 0.1f, 0.0f, 300.0f)) {
      decChanged = true;
    }
    if (decChanged) {
      edits.PushDecal(e->id, dec);
    }
    DrawDecalSection(e);
  }
  if (e->hasLod) {
    ImGui::Separator();
    ImGui::Text("LOD");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##lod")) {
      std::unique_ptr<Command> cmd(new RemoveLodCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    LodData lod = e->lod;
    bool lodChanged = false;
    if (ImGui::DragFloat("Near Dist", &lod.nearDist, 0.5f, 1.0f, 500.0f)) {
      lodChanged = true;
    }
    if (ImGui::DragFloat("Far Dist", &lod.farDist, 0.5f, 2.0f, 1000.0f)) {
      lodChanged = true;
    }
    if (lodChanged) {
      edits.PushLod(e->id, lod);
    }
    if (lod.farGuid.empty()) {
      ImGui::TextDisabled("Far mesh: (none)");
    } else {
      const MeshRecord* fr = assets.GetByGuid(lod.farGuid);
      if (fr == nullptr) {
        ImGui::TextDisabled("Far mesh: (missing)");
      } else {
        std::filesystem::path fp(fr->meshPath);
        ImGui::Text("Far mesh: %s", fp.filename().string().c_str());
      }
    }
    if (ImGui::BeginDragDropTarget()) {
      const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ASSET");
      if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
        std::string guid(static_cast<const char*>(payload->Data));
        if (assets.GetByGuid(guid) == nullptr) {
          Announce(LogLevel::Warning, "Dropped asset not found");
        } else if (guid != lod.farGuid) {
          LodData after = e->lod;
          after.farGuid = guid;
          std::unique_ptr<Command> cmd(new SetLodCmd(e->id, e->lod, after));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, "LOD far mesh assigned");
          }
        }
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##lodfar")) {
      if (!e->lod.farGuid.empty()) {
        LodData after = e->lod;
        after.farGuid.clear();
        std::unique_ptr<Command> cmd(new SetLodCmd(e->id, e->lod, after));
        undo.Execute(std::move(cmd), scene);
      }
    }
    if (e->lod.farGuid.empty() && !e->meshGuid.empty()) {
      if (ImGui::Button("Generate LOD")) {
        const MeshRecord* base = assets.GetByGuid(e->meshGuid);
        if (base == nullptr || !base->loaded) {
          Announce(LogLevel::Warning, "Base mesh not loaded");
        } else {
          Kizuri::MeshAssetData lodMesh;
          if (!Kizuri::GenerateLodMesh(base->data, 0.4f, 0.01f, lodMesh)) {
            Announce(LogLevel::Error, "LOD generation failed");
          } else {
            std::filesystem::path bp(base->meshPath);
            std::string lodName = bp.stem().string() + "_lod1.kzmesh";
            std::string lodPath = (bp.parent_path() / lodName).string();
            if (!Kizuri::EncodeMeshFile(lodMesh, lodPath)) {
              Announce(LogLevel::Error, "LOD save failed");
            } else {
              assets.Scan();
              LodData after = e->lod;
              after.farGuid = lodMesh.guid;
              std::unique_ptr<Command> cmd(new SetLodCmd(e->id, e->lod, after));
              if (undo.Execute(std::move(cmd), scene)) {
                log.Add(LogLevel::Info, "LOD generated");
              }
            }
          }
        }
      }
    }
  }
  if (e->hasLightmap) {
    ImGui::Separator();
    ImGui::Text("Lightmap");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##lightmap")) {
      std::unique_ptr<Command> cmd(new RemoveLightmapCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    LightmapData lm = e->lightmap;
    bool lmChanged = false;
    if (ImGui::DragFloat("Lightmap Intensity", &lm.intensity, 0.05f, 0.0f, 4.0f)) {
      lmChanged = true;
    }
    if (lmChanged) {
      edits.PushLightmap(e->id, lm);
    }
    if (lm.texGuid.empty()) {
      ImGui::TextDisabled("Texture: (none)");
    } else {
      const TextureRecord* tr = assets.GetTexByGuid(lm.texGuid);
      if (tr == nullptr) {
        ImGui::TextDisabled("Texture: (missing)");
      } else {
        std::filesystem::path tp(tr->texPath);
        ImGui::Text("Texture: %s", tp.filename().string().c_str());
      }
    }
    if (ImGui::BeginDragDropTarget()) {
      const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_TEX");
      if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
        std::string guid(static_cast<const char*>(payload->Data));
        if (assets.GetTexByGuid(guid) == nullptr) {
          Announce(LogLevel::Warning, "Dropped texture not found");
        } else if (guid != lm.texGuid) {
          LightmapData after = e->lightmap;
          after.texGuid = guid;
          std::unique_ptr<Command> cmd(new SetLightmapCmd(e->id, e->lightmap, after));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, "Lightmap texture assigned");
          }
        }
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##lightmaptex")) {
      if (!e->lightmap.texGuid.empty()) {
        LightmapData after = e->lightmap;
        after.texGuid.clear();
        std::unique_ptr<Command> cmd(new SetLightmapCmd(e->id, e->lightmap, after));
        undo.Execute(std::move(cmd), scene);
      }
    }
    if (ImGui::Button("Bake Lightmap")) {
      BakeLightmap(e->id);
    }
  }
  if (e->hasProbe) {
    ImGui::Separator();
    ImGui::Text("Reflection Probe");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##probe")) {
      std::unique_ptr<Command> cmd(new RemoveProbeCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    ProbeData pr = e->probe;
    bool prChanged = false;
    if (ImGui::DragFloat3("Probe Size", pr.size, 0.1f, 0.5f, 100.0f)) {
      prChanged = true;
    }
    if (ImGui::DragFloat("Probe Intensity", &pr.intensity, 0.05f, 0.0f, 4.0f)) {
      prChanged = true;
    }
    if (prChanged) {
      edits.PushProbe(e->id, pr);
    }
    auto pit = envProbeCubes.find(e->id);
    if (pit == envProbeCubes.end() || pit->second == 0) {
      ImGui::TextDisabled("Capture: (not baked)");
    } else {
      ImGui::Text("Capture: baked");
    }
    if (ImGui::Button("Bake Probe")) {
      BakeProbeEntity(e->id);
    }
  }
  if (!e->hasMesh || !e->hasLight || !e->hasMaterial || !e->hasDecal || !e->hasLod || !e->hasLightmap || !e->hasProbe) {
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("AddComponent");
    }
    if (ImGui::BeginPopup("AddComponent")) {
      if (!e->hasMesh && ImGui::MenuItem("Mesh Renderer")) {
        std::unique_ptr<Command> cmd(new AddMeshCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasLight && ImGui::MenuItem("Light")) {
        std::unique_ptr<Command> cmd(new AddLightCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasMaterial && ImGui::MenuItem("Material")) {
        MaterialData initial;
        MakeDefaultMaterial(initial);
        if (e->hasMesh && !e->meshGuid.empty()) {
          const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
          if (rec != nullptr && rec->loaded && !rec->data.materials.empty()) {
            size_t mi = 0;
            if (!rec->data.parts.empty() && rec->data.parts[0].material < rec->data.materials.size()) {
              mi = rec->data.parts[0].material;
            }
            MaterialDataFromMesh(rec->data.materials[mi], initial);
            if (rec->data.materials.size() > 1) {
              std::string mname = rec->data.materials[mi].name.empty() ? ("Material " + std::to_string(mi)) : rec->data.materials[mi].name;
              Announce(LogLevel::Warning, "Mesh has " + std::to_string(rec->data.materials.size()) + " materials; override pre-filled from '" + mname + "' and applies to all parts");
            }
          }
        }
        std::unique_ptr<Command> cmd(new AddMaterialCmd(e->id, initial));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasDecal && ImGui::MenuItem("Decal")) {
        std::unique_ptr<Command> cmd(new AddDecalCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasLod && ImGui::MenuItem("LOD")) {
        std::unique_ptr<Command> cmd(new AddLodCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasLightmap && ImGui::MenuItem("Lightmap")) {
        std::unique_ptr<Command> cmd(new AddLightmapCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasProbe && ImGui::MenuItem("Probe")) {
        std::unique_ptr<Command> cmd(new AddProbeCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();
}
void EditorApp::DrawConsole() {
  ImGui::Begin("Console", &showConsole);
  if (ImGui::Button("Clear")) {
    log.Clear();
  }
  ImGui::SameLine();
  ImGui::Text("%llu messages", (unsigned long long)log.Count());
  ImGui::SameLine();
  if (ImGui::Button(showNotifHistory ? "Hide Notifications" : "Notifications")) {
    showNotifHistory = !showNotifHistory;
  }
  ImGui::Separator();
  if (showNotifHistory) {
    std::vector<Notification> hist = notifications.History();
    ImGui::BeginChild("NotifHistory", ImVec2(0, 120), true);
    for (size_t i = 0; i < hist.size(); ++i) {
      ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
      const char* tag = "INFO";
      if (hist[i].level == LogLevel::Success) {
        color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        tag = "OK";
      } else if (hist[i].level == LogLevel::Warning) {
        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        tag = "WARN";
      } else if (hist[i].level == LogLevel::Error) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        tag = "ERROR";
      }
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s] %s", tag, hist[i].text.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::Separator();
  }
  ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  for (size_t i = 0; i < log.Count(); ++i) {
    const LogEntry& entry = log.At(i);
    ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
    const char* tag = "INFO";
    if (entry.level == LogLevel::Success) {
      color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
      tag = "OK";
    } else if (entry.level == LogLevel::Warning) {
      color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
      tag = "WARN";
    } else if (entry.level == LogLevel::Error) {
      color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
      tag = "ERROR";
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("[%s] %s", tag, entry.text.c_str());
    ImGui::PopStyleColor();
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
  ImGui::End();
}
void EditorApp::DrawMeshSection(Entity* e) {
  if (e == nullptr) {
    return;
  }
  if (e->meshGuid.empty()) {
    ImGui::Text("Mesh: (none)");
  } else {
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec == nullptr) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      ImGui::Text("Missing asset");
      ImGui::PopStyleColor();
      ImGui::Text("Drop an asset here to reassign");
    } else if (!rec->loaded || !rec->gpuReady) {
      ImGui::Text("Mesh: loading...");
    } else {
      std::filesystem::path mp(rec->meshPath);
      ImGui::Text("Mesh: %s", mp.filename().string().c_str());
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ASSET");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      if (assets.GetByGuid(guid) == nullptr) {
        Announce(LogLevel::Warning, "Dropped asset not found");
      } else if (guid != e->meshGuid) {
        std::unique_ptr<Command> cmd(new SetMeshGuidCmd(e->id, e->meshGuid, guid));
        if (undo.Execute(std::move(cmd), scene)) {
          log.Add(LogLevel::Info, "Mesh assigned");
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Mesh")) {
    if (!e->meshGuid.empty()) {
      std::unique_ptr<Command> cmd(new SetMeshGuidCmd(e->id, e->meshGuid, ""));
      undo.Execute(std::move(cmd), scene);
    }
  }
  if (!e->meshGuid.empty() && !e->hasMaterial) {
    const MeshRecord* mrec = assets.GetByGuid(e->meshGuid);
    if (mrec != nullptr && mrec->loaded && !mrec->data.materials.empty()) {
      if (ImGui::CollapsingHeader("Mesh Materials")) {
        for (size_t mi = 0; mi < mrec->data.materials.size(); ++mi) {
          const MeshMaterialData& mm = mrec->data.materials[mi];
          std::string mlabel = mm.name.empty() ? ("Material " + std::to_string(mi)) : mm.name;
          ImGui::Text("%s", mlabel.c_str());
          ImGui::SameLine();
          ImGui::TextDisabled("(%.2g, %.2g, %.2g)", mm.albedo[0], mm.albedo[1], mm.albedo[2]);
          if (mm.unlit) {
            ImGui::SameLine();
            ImGui::TextDisabled("[unlit]");
          }
          if (mm.emissive[0] > 0.0f || mm.emissive[1] > 0.0f || mm.emissive[2] > 0.0f) {
            ImGui::BulletText("Emissive factor: (%.2g, %.2g, %.2g) x%.2g%s", mm.emissive[0], mm.emissive[1], mm.emissive[2], mm.emissiveStrength, mm.emissiveTexGuid.empty() ? " (no texture: glows flat)" : "");
          }
          const char* slotNames[5] = { "Albedo", "Normal", "ORM", "Height", "Emissive" };
          const std::string* slotGuids[5] = { &mm.albedoTexGuid, &mm.normalTexGuid, &mm.ormTexGuid, &mm.heightTexGuid, &mm.emissiveTexGuid };
          for (int s = 0; s < 5; ++s) {
            if (slotGuids[s]->empty()) {
              continue;
            }
            const TextureRecord* tr = assets.GetTexByGuid(*slotGuids[s]);
            if (tr == nullptr) {
              ImGui::BulletText("%s: (importing...)", slotNames[s]);
            } else {
              std::filesystem::path tp(tr->texPath);
              std::string st = std::string(slotNames[s]) + ": " + tp.filename().string();
              if (!tr->loaded || tr->gpu == 0) {
                st += " (loading...)";
              }
              ImGui::BulletText("%s", st.c_str());
            }
          }
        }
      }
    }
  }
  if (!e->meshGuid.empty()) {
    const MeshRecord* lrec = assets.GetByGuid(e->meshGuid);
    if (lrec != nullptr && lrec->loaded && !lrec->data.lights.empty()) {
      std::string lab = "Import lights (" + std::to_string(lrec->data.lights.size()) + ")";
      if (ImGui::Button(lab.c_str())) {
        for (size_t li = 0; li < lrec->data.lights.size(); ++li) {
          const MeshLightData& ml = lrec->data.lights[li];
          Transform t;
          MakeIdentityTransform(t);
          t.position[0] = ml.matrix[12];
          t.position[1] = ml.matrix[13];
          t.position[2] = -ml.matrix[14];
          float dx = -ml.matrix[8];
          float dy = -ml.matrix[9];
          float dz = ml.matrix[10];
          float dl = sqrtf(dx * dx + dy * dy + dz * dz);
          if (dl > 1e-6f) {
            dx /= dl;
            dy /= dl;
            dz /= dl;
            t.rotation[0] = -asinf(dy > 1.0f ? 1.0f : (dy < -1.0f ? -1.0f : dy)) * 57.29577951f;
            t.rotation[1] = atan2f(dx, dz) * 57.29577951f;
            t.rotation[2] = 0.0f;
          }
          std::string lname = ml.name.empty() ? "Light" : ml.name;
          std::unique_ptr<Command> cc(new CreateEntityCmd(lname, t, EntityId::Invalid(), ""));
          std::vector<EntityId> beforeIds = scene.All();
          if (!undo.Execute(std::move(cc), scene)) {
            continue;
          }
          EntityId nid;
          bool found = false;
          for (size_t ai = 0; ai < scene.All().size(); ++ai) {
            EntityId cand = scene.All()[ai];
            bool old = false;
            for (size_t bi = 0; bi < beforeIds.size(); ++bi) {
              if (cand == beforeIds[bi]) {
                old = true;
                break;
              }
            }
            if (!old) {
              nid = cand;
              found = true;
              break;
            }
          }
          if (!found) {
            continue;
          }
          std::unique_ptr<Command> ac(new AddLightCmd(nid));
          if (!undo.Execute(std::move(ac), scene)) {
            continue;
          }
          Entity* ne = scene.Get(nid);
          if (ne == nullptr) {
            continue;
          }
          LightData after = ne->light;
          after.type = ml.type < 0 ? 0 : (ml.type > 2 ? 2 : ml.type);
          after.color[0] = ml.color[0];
          after.color[1] = ml.color[1];
          after.color[2] = ml.color[2];
          after.intensity = ml.intensity;
          if (ml.range > 0.0f) {
            after.radius = ml.range;
          }
          if (ml.type == 1) {
            after.spotAngle = ml.outerCone * 57.29577951f;
            if (after.spotAngle < 1.0f) {
              after.spotAngle = 1.0f;
            }
            if (after.spotAngle > 170.0f) {
              after.spotAngle = 170.0f;
            }
          }
          std::unique_ptr<Command> sc(new SetLightCmd(nid, ne->light, after));
          undo.Execute(std::move(sc), scene);
        }
        log.Add(LogLevel::Info, "Imported lights from mesh");
      }
    }
    if (lrec != nullptr && lrec->loaded && !lrec->data.variantNames.empty()) {
      int cur = e->variantIndex;
      std::string preview = cur < 0 ? "Default" : (static_cast<size_t>(cur) < lrec->data.variantNames.size() ? lrec->data.variantNames[static_cast<size_t>(cur)] : "Default");
      if (ImGui::BeginCombo("Variant", preview.c_str())) {
        bool selDef = cur < 0;
        if (ImGui::Selectable("Default", selDef)) {
          if (cur >= 0) {
            std::unique_ptr<Command> cmd(new SetVariantCmd(e->id, cur, -1));
            undo.Execute(std::move(cmd), scene);
          }
        }
        for (size_t vi = 0; vi < lrec->data.variantNames.size(); ++vi) {
          bool sel = cur == static_cast<int>(vi);
          if (ImGui::Selectable(lrec->data.variantNames[vi].c_str(), sel)) {
            if (!sel) {
              std::unique_ptr<Command> cmd(new SetVariantCmd(e->id, cur, static_cast<int>(vi)));
              undo.Execute(std::move(cmd), scene);
            }
          }
        }
        ImGui::EndCombo();
      }
    }
  }
  DrawPartMaterials(e);
}
bool EditorApp::DrawMaterialScalars(MaterialData& mat) {
  bool matChanged = false;
    if (ImGui::ColorEdit3("Albedo", mat.albedo)) {
      matChanged = true;
    }
    if (ImGui::DragFloat("Alpha", &mat.alpha, 0.01f, 0.0f, 1.0f)) {
      matChanged = true;
    }
    if (ImGui::DragFloat("Metallic", &mat.metallic, 0.01f, 0.0f, 1.0f)) {
      matChanged = true;
    }
    if (ImGui::DragFloat("Roughness", &mat.roughness, 0.01f, 0.0f, 1.0f)) {
      matChanged = true;
    }
    if (ImGui::Checkbox("Unlit", &mat.unlit)) {
      matChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Double Sided", &mat.doubleSided)) {
      matChanged = true;
    }
    int alphaMode = mat.alphaMode;
    if (ImGui::Combo("Alpha Mode", &alphaMode, "Opaque\0Mask\0Blend\0")) {
      mat.alphaMode = alphaMode < 0 ? 0 : (alphaMode > 2 ? 2 : alphaMode);
      matChanged = true;
    }
    if (ImGui::DragFloat("Alpha Cutoff", &mat.alphaCutoff, 0.01f, 0.0f, 1.0f)) {
      matChanged = true;
    }
    if (ImGui::CollapsingHeader("Emissive")) {
      if (ImGui::ColorEdit3("Emissive Color", mat.emissive)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Emissive Strength", &mat.emissiveStrength, 0.05f, 0.0f, 16.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Specular (IOR)")) {
      if (ImGui::DragFloat("Specular IOR", &mat.ior, 0.01f, 1.0f, 3.0f)) {
        matChanged = true;
      }
      if (ImGui::Checkbox("Has Specular Ext", &mat.hasSpecular)) {
        matChanged = true;
      }
      if (ImGui::ColorEdit3("Spec Color", mat.specularColor)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Spec Strength", &mat.specularStrength, 0.01f, 0.0f, 4.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Clearcoat")) {
      if (ImGui::DragFloat("Clearcoat Factor", &mat.clearcoat, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Clearcoat Roughness", &mat.clearcoatRoughness, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Sheen")) {
      if (ImGui::ColorEdit3("Sheen Color", mat.sheenColor)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Sheen Roughness", &mat.sheenRoughness, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Anisotropy")) {
      if (ImGui::DragFloat("Aniso Strength", &mat.anisotropy, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Aniso Rotation", &mat.anisotropyRotation, 0.01f, -3.15f, 3.15f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Iridescence")) {
      if (ImGui::DragFloat("Iridescence Factor", &mat.iridescence, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Iridescence IOR", &mat.iridescenceIor, 0.01f, 1.0f, 2.5f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Thick Min", &mat.iridescenceThicknessMin, 1.0f, 10.0f, 1000.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Thick Max", &mat.iridescenceThicknessMax, 1.0f, 10.0f, 2000.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Transmission / Volume")) {
      if (ImGui::DragFloat("Transmission", &mat.transmission, 0.01f, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Thickness", &mat.thickness, 0.01f, 0.0f, 10.0f)) {
        matChanged = true;
      }
      if (ImGui::ColorEdit3("Attenuation", mat.attenuationColor)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Atten Dist", &mat.attenuationDistance, 0.05f, 0.0f, 100.0f)) {
        matChanged = true;
      }
      if (ImGui::DragFloat("Dispersion", &mat.dispersion, 0.005f, 0.0f, 1.0f)) {
        matChanged = true;
      }
    }
  return matChanged;
}

void EditorApp::DrawMaterialSection(Entity* e) {
  if (e == nullptr || !e->hasMaterial) {
    return;
  }
  DrawMaterialSlot(e, "Albedo", 0);
  DrawMaterialSlot(e, "Normal", 1);
  DrawMaterialSlot(e, "ORM", 2);
  DrawMaterialSlot(e, "Height", 3);
  DrawMaterialSlot(e, "Emissive", 4);
  DrawMaterialSlot(e, "Specular", 5);
  DrawMaterialSlot(e, "Spec Color", 6);
  DrawMaterialSlot(e, "Clearcoat", 7);
  DrawMaterialSlot(e, "Clearcoat Rough", 8);
  DrawMaterialSlot(e, "Clearcoat Normal", 9);
  DrawMaterialSlot(e, "Sheen Color", 10);
  DrawMaterialSlot(e, "Sheen Rough", 11);
  DrawMaterialSlot(e, "Anisotropy", 12);
  DrawMaterialSlot(e, "Iridescence", 13);
  DrawMaterialSlot(e, "Iridescence Thick", 14);
  DrawMaterialSlot(e, "Transmission", 15);
  DrawMaterialSlot(e, "Thickness", 16);
}
void EditorApp::DrawPartMaterials(Entity* e) {
  if (e == nullptr || !e->hasMesh || e->meshGuid.empty()) {
    return;
  }
  const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
  if (rec == nullptr || !rec->loaded || rec->data.parts.empty()) {
    return;
  }
  if (!ImGui::CollapsingHeader("Part Materials")) {
    return;
  }
  for (size_t p = 0; p < rec->data.parts.size(); ++p) {
    const MeshPartData& part = rec->data.parts[p];
    ImGui::PushID(static_cast<int>(p));
    std::string baseName = "mesh";
    if (part.material < rec->data.materials.size()) {
      baseName = rec->data.materials[part.material].name.empty() ? ("Material " + std::to_string(part.material)) : rec->data.materials[part.material].name;
    }
    MaterialData* ov = nullptr;
    for (size_t k = 0; k < e->partMaterials.size(); ++k) {
      if (e->partMaterials[k].part == static_cast<uint32_t>(p)) {
        ov = &e->partMaterials[k].material;
        break;
      }
    }
    std::string head = "Part " + std::to_string(p) + " (base: " + baseName + (ov != nullptr ? ", override)" : ")");
    if (e->hasMaterial && ov == nullptr) {
      head = "Part " + std::to_string(p) + " (entity override)";
    }
    ImGui::Text("%s", head.c_str());
    ImGui::SameLine();
    if (ov != nullptr) {
      if (ImGui::SmallButton("Clear##partoverride")) {
        std::unique_ptr<Command> cmd(new RemovePartMaterialCmd(e->id, static_cast<uint32_t>(p)));
        undo.Execute(std::move(cmd), scene);
        ImGui::PopID();
        continue;
      }
      MaterialData* live = nullptr;
      for (size_t k = 0; k < e->partMaterials.size(); ++k) {
        if (e->partMaterials[k].part == static_cast<uint32_t>(p)) {
          live = &e->partMaterials[k].material;
          break;
        }
      }
      if (live == nullptr) {
        ImGui::PopID();
        continue;
      }
      MaterialData mat = *live;
      if (DrawMaterialScalars(mat)) {
        edits.PushPartMaterial(e->id, static_cast<uint32_t>(p), mat);
      }
      DrawMaterialSlotFor(e, "Albedo", 0, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Normal", 1, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "ORM", 2, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Height", 3, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Emissive", 4, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Specular", 5, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Spec Color", 6, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Clearcoat", 7, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Clearcoat Rough", 8, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Clearcoat Normal", 9, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Sheen Color", 10, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Sheen Rough", 11, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Anisotropy", 12, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Iridescence", 13, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Iridescence Thick", 14, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Transmission", 15, true, static_cast<uint32_t>(p));
      DrawMaterialSlotFor(e, "Thickness", 16, true, static_cast<uint32_t>(p));
    } else {
      std::string btn = "Override##part" + std::to_string(p);
      if (ImGui::SmallButton(btn.c_str())) {
        MaterialData initial;
        if (e->hasMaterial) {
          initial = e->material;
        } else if (part.material < rec->data.materials.size()) {
          uint32_t matIdx = part.material;
          if (e->variantIndex >= 0) {
            for (size_t vi = 0; vi < rec->data.variantAssigns.size(); ++vi) {
              const MeshVariantAssign& va = rec->data.variantAssigns[vi];
              if (va.part == static_cast<uint32_t>(p) && static_cast<int>(va.variant) == e->variantIndex && va.material < rec->data.materials.size()) {
                matIdx = va.material;
                break;
              }
            }
          }
          MaterialDataFromMesh(rec->data.materials[matIdx], initial);
        } else {
          MakeDefaultMaterial(initial);
        }
        std::unique_ptr<Command> cmd(new AddPartMaterialCmd(e->id, static_cast<uint32_t>(p), initial));
        undo.Execute(std::move(cmd), scene);
      }
    }
    ImGui::PopID();
  }
}
void EditorApp::DrawDecalSection(Entity* e) {
  if (e == nullptr || !e->hasDecal) {
    return;
  }
  ImGui::Text("Texture:");
  ImGui::SameLine();
  if (e->decal.texGuid.empty()) {
    ImGui::TextDisabled("(none)");
  } else {
    const TextureRecord* tr = assets.GetTexByGuid(e->decal.texGuid);
    if (tr == nullptr) {
      ImGui::TextDisabled("(missing)");
    } else {
      std::filesystem::path tp(tr->texPath);
      ImGui::Text("%s", tp.filename().string().c_str());
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_TEX");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      if (assets.GetTexByGuid(guid) == nullptr) {
        Announce(LogLevel::Warning, "Dropped texture not found");
      } else if (guid != e->decal.texGuid) {
        DecalData after = e->decal;
        after.texGuid = guid;
        std::unique_ptr<Command> cmd(new SetDecalCmd(e->id, e->decal, after));
        if (undo.Execute(std::move(cmd), scene)) {
          log.Add(LogLevel::Info, "Decal texture assigned");
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear##decaltex")) {
    if (!e->decal.texGuid.empty()) {
      DecalData after = e->decal;
      after.texGuid.clear();
      std::unique_ptr<Command> cmd(new SetDecalCmd(e->id, e->decal, after));
      undo.Execute(std::move(cmd), scene);
    }
  }
}
static std::string* SlotPtr(MaterialData& m, int slot) {
  if (slot == 0) {
    return &m.albedoTex;
  } else if (slot == 1) {
    return &m.normalTex;
  } else if (slot == 2) {
    return &m.ormTex;
  } else if (slot == 3) {
    return &m.heightTex;
  } else if (slot == 4) {
    return &m.emissiveTex;
  } else if (slot == 5) {
    return &m.specularTex;
  } else if (slot == 6) {
    return &m.specularColorTex;
  } else if (slot == 7) {
    return &m.clearcoatTex;
  } else if (slot == 8) {
    return &m.clearcoatRoughnessTex;
  } else if (slot == 9) {
    return &m.clearcoatNormalTex;
  } else if (slot == 10) {
    return &m.sheenColorTex;
  } else if (slot == 11) {
    return &m.sheenRoughnessTex;
  } else if (slot == 12) {
    return &m.anisotropyTex;
  } else if (slot == 13) {
    return &m.iridescenceTex;
  } else if (slot == 14) {
    return &m.iridescenceThicknessTex;
  } else if (slot == 15) {
    return &m.transmissionTex;
  } else if (slot == 16) {
    return &m.thicknessTex;
  }
  return nullptr;
}
void EditorApp::DrawMaterialSlot(Entity* e, const char* label, int slot) {
  if (e == nullptr) {
    return;
  }
  DrawMaterialSlotFor(e, label, slot, false, 0);
}
void EditorApp::DrawMaterialSlotFor(Entity* e, const char* label, int slot, bool isPart, uint32_t part) {
  if (e == nullptr || label == nullptr) {
    return;
  }
  MaterialData* live = &e->material;
  if (isPart) {
    live = nullptr;
    for (size_t k = 0; k < e->partMaterials.size(); ++k) {
      if (e->partMaterials[k].part == part) {
        live = &e->partMaterials[k].material;
        break;
      }
    }
    if (live == nullptr) {
      return;
    }
  } else if (!e->hasMaterial) {
    return;
  }
  std::string* curPtr = SlotPtr(*live, slot);
  if (curPtr == nullptr) {
    return;
  }
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  if (curPtr->empty()) {
    ImGui::TextDisabled("(none)");
  } else {
    const TextureRecord* tr = assets.GetTexByGuid(*curPtr);
    if (tr == nullptr) {
      ImGui::TextDisabled("(missing)");
    } else {
      std::filesystem::path tp(tr->texPath);
      ImGui::Text("%s", tp.filename().string().c_str());
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_TEX");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      if (assets.GetTexByGuid(guid) == nullptr) {
        Announce(LogLevel::Warning, "Dropped texture not found");
      } else if (guid != *curPtr) {
        if ((slot == 1 || slot == 2) && !assets.EnsureTextureKind(guid, slot == 1 ? TexKind::Normal : TexKind::Orm)) {
          Announce(LogLevel::Warning, "Texture has no source; assigned without conversion");
        }
        MaterialData after = *live;
        std::string* dst = SlotPtr(after, slot);
        if (dst == nullptr) {
          ImGui::EndDragDropTarget();
          return;
        }
        *dst = guid;
        if (isPart) {
          std::unique_ptr<Command> cmd(new SetPartMaterialCmd(e->id, part, *live, after));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, "Part material texture assigned");
          }
        } else {
          std::unique_ptr<Command> cmd(new SetMaterialCmd(e->id, *live, after));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, "Material texture assigned");
          }
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::SameLine();
  std::string clearLabel = std::string("Clear##") + label;
  if (ImGui::SmallButton(clearLabel.c_str())) {
    if (!curPtr->empty()) {
      MaterialData after = *live;
      std::string* dst = SlotPtr(after, slot);
      if (dst == nullptr) {
        return;
      }
      dst->clear();
      if (isPart) {
        std::unique_ptr<Command> cmd(new SetPartMaterialCmd(e->id, part, *live, after));
        undo.Execute(std::move(cmd), scene);
      } else {
        std::unique_ptr<Command> cmd(new SetMaterialCmd(e->id, *live, after));
        undo.Execute(std::move(cmd), scene);
      }
    }
  }
}
void EditorApp::DrawAssetBrowser() {
  ImGui::Begin("Asset Browser", &showAssetBrowser);
  if (ImGui::Button("Refresh")) {
    assets.Scan();
  }
  ImGui::SameLine();
  ImGui::Text("%s", assets.AssetsDir().c_str());
  std::vector<std::pair<std::string, int>> importing = assets.ImportingNow();
  for (size_t i = 0; i < importing.size(); ++i) {
    ImGui::Text("Importing %s... %d%%", importing[i].first.c_str(), importing[i].second);
  }
  ImGui::Separator();
  ImGui::Text("Meshes:");
  std::vector<std::string> guids = assets.AllGuids();
  for (size_t i = 0; i < guids.size(); ++i) {
    const MeshRecord* rec = assets.GetByGuid(guids[i]);
    if (rec == nullptr) {
      continue;
    }
    std::filesystem::path mp(rec->meshPath);
    std::string label = "M " + mp.filename().string();
    ImGui::PushID(static_cast<int>(i));
    ImGui::Selectable(label.c_str(), selectedAssetGuid == rec->guid && !selectedAssetIsTex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      selectedAssetGuid = rec->guid;
      selectedAssetIsTex = false;
    }
    if (rec->state == MeshAssetState::Outdated) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[OUTDATED]");
    } else if (rec->state == MeshAssetState::NoSource) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[NO SOURCE]");
    } else if (rec->state == MeshAssetState::SourceMissing) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[SRC MISSING]");
    } else if (!rec->loaded) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[LOADING]");
    }
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("KZ_ASSET", rec->guid.c_str(), rec->guid.size() + 1);
      ImGui::Text("%s", mp.filename().string().c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("MeshCtx")) {
      bool canReimport = rec->hasSource;
      if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
        if (assets.Reimport(rec->guid)) {
          Announce(LogLevel::Info, std::string("Reimporting ") + mp.filename().string());
        } else {
          Announce(LogLevel::Warning, "No source to reimport from");
        }
      }
      if (ImGui::MenuItem("Rename")) {
        assetRenameActive = true;
        assetRenameIsTex = false;
        assetRenameGuid = rec->guid;
        std::string fn = mp.filename().string();
        std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
        assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Delete")) {
        std::vector<RefUse> refs = SceneMeshRefs();
        std::vector<std::string> blocked;
        if (assets.DeleteAssetFile(rec->guid, false, refs, blocked)) {
          Announce(LogLevel::Warning, std::string("Deleted ") + mp.filename().string());
        } else {
          std::string msg = "Cannot delete '" + mp.filename().string() + "': used by " + std::to_string(blocked.size()) + " (";
          for (size_t b = 0; b < blocked.size(); ++b) {
            if (b > 0) {
              msg += ", ";
            }
            msg += blocked[b];
          }
          msg += ")";
          Announce(LogLevel::Error, msg);
        }
      }
      if (ImGui::MenuItem("Force Delete")) {
        forceDeleteGuid = rec->guid;
        forceDeleteIsTex = false;
        ImGui::OpenPopup("Force Delete?");
      }
      if (ImGui::MenuItem("Show in folder")) {
        std::string dir = mp.parent_path().string();
        ShellExecuteW(nullptr, L"open", std::filesystem::path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOW);
      }
      if (rec->state == MeshAssetState::SourceMissing) {
        if (ImGui::MenuItem("Locate source...")) {
          std::string picked;
          if (ShowOpenGltfDialog(window.NativeHandle(), picked)) {
            if (assets.SetSourcePath(rec->guid, picked)) {
              Announce(LogLevel::Success, "Source relinked");
            } else {
              Announce(LogLevel::Error, "Could not relink source");
            }
          }
        }
      }
      ImGui::EndPopup();
    }
    if (assetRenameActive && !assetRenameIsTex && assetRenameGuid == rec->guid) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##assetrename", assetRenameBuf, sizeof(assetRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!assets.RenameAssetFile(rec->guid, assetRenameBuf)) {
          Announce(LogLevel::Error, "Rename failed");
        }
        assetRenameActive = false;
      }
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::Text("Textures:");
  std::vector<std::string> tguids = assets.AllTexGuids();
  for (size_t i = 0; i < tguids.size(); ++i) {
    const TextureRecord* rec = assets.GetTexByGuid(tguids[i]);
    if (rec == nullptr) {
      continue;
    }
    std::filesystem::path tp(rec->texPath);
    std::string label = "T " + tp.filename().string() + " (" + std::to_string(rec->data.width) + "x" + std::to_string(rec->data.height) + " " + std::to_string(rec->data.mips.size()) + "mip)";
    ImGui::PushID(static_cast<int>(100000 + i));
    ImGui::Selectable(label.c_str(), selectedAssetGuid == rec->guid && selectedAssetIsTex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      selectedAssetGuid = rec->guid;
      selectedAssetIsTex = true;
    }
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("KZ_TEX", rec->guid.c_str(), rec->guid.size() + 1);
      ImGui::Text("%s", tp.filename().string().c_str());
      ImGui::EndDragDropSource();
    }
    if (rec->state == TextureAssetState::Outdated) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[OUTDATED]");
    } else if (rec->state == TextureAssetState::NoSource) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[NO SOURCE]");
    } else if (rec->state == TextureAssetState::SourceMissing) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[SRC MISSING]");
    } else if (!rec->loaded) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[LOADING]");
    }
    ImGui::SameLine();
    ImGui::Text("gpu:%d/%llu", rec->residentLevels, (unsigned long long)rec->data.mips.size());
    if (ImGui::BeginPopupContextItem("TexCtx")) {
      bool canReimport = rec->hasSource;
      if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
        if (assets.ReimportTexture(rec->guid)) {
          Announce(LogLevel::Info, std::string("Reimporting ") + tp.filename().string());
        } else {
          Announce(LogLevel::Warning, "No source to reimport from");
        }
      }
      if (ImGui::MenuItem("Rename")) {
        assetRenameActive = true;
        assetRenameIsTex = true;
        assetRenameGuid = rec->guid;
        std::string fn = tp.filename().string();
        std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
        assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Delete")) {
        std::vector<std::string> blocked;
        if (assets.DeleteTextureFile(rec->guid, false, blocked)) {
          Announce(LogLevel::Warning, std::string("Deleted ") + tp.filename().string());
        } else {
          std::string msg = "Cannot delete '" + tp.filename().string() + "': used by " + std::to_string(blocked.size()) + " (";
          for (size_t b = 0; b < blocked.size(); ++b) {
            if (b > 0) {
              msg += ", ";
            }
            msg += blocked[b];
          }
          msg += ")";
          Announce(LogLevel::Error, msg);
        }
      }
      if (ImGui::MenuItem("Force Delete")) {
        forceDeleteGuid = rec->guid;
        forceDeleteIsTex = true;
        ImGui::OpenPopup("Force Delete?");
      }
      if (ImGui::MenuItem("Show in folder")) {
        std::string dir = tp.parent_path().string();
        ShellExecuteW(nullptr, L"open", std::filesystem::path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOW);
      }
      ImGui::EndPopup();
    }
    if (assetRenameActive && assetRenameIsTex && assetRenameGuid == rec->guid) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##texrename", assetRenameBuf, sizeof(assetRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!assets.RenameTextureFile(rec->guid, assetRenameBuf)) {
          Announce(LogLevel::Error, "Rename failed");
        }
        assetRenameActive = false;
      }
    }
    ImGui::PopID();
  }
  if (ImGui::BeginPopupModal("Force Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Force delete? Dependents will show missing asset.");
    if (ImGui::Button("Force")) {
      std::vector<RefUse> refs = SceneMeshRefs();
      std::vector<std::string> blocked;
      if (forceDeleteIsTex) {
        assets.DeleteTextureFile(forceDeleteGuid, true, blocked);
      } else {
        assets.DeleteAssetFile(forceDeleteGuid, true, refs, blocked);
      }
      Announce(LogLevel::Warning, "Asset force deleted");
      forceDeleteGuid.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      forceDeleteGuid.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
}
