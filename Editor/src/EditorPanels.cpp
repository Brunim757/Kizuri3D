#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Assets/EmbeddedSkies.h"
#include "Kizuri/Assets/LightmapBaker.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/QualityPreset.h"
#include "Kizuri/Path.h"
#include "FileDialog.h"
#include "Kizuri/DebugDraw.h"
#include "Kizuri/Input.h"
#include "Kizuri/Picking.h"
#include <windows.h>
#include <shellapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <map>
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
  partMat.linkGroup = static_cast<uint32_t>(m.linkMask < 0 ? 0 : (m.linkMask > 255 ? 255 : m.linkMask));
  std::string nodeAlbedoTex = m.albedoTex;
  float nodeAlbedo[3] = { m.albedo[0], m.albedo[1], m.albedo[2] };
  float nodeRough = m.roughness;
  float nodeMetal = m.metallic;
  float nodeEmissive[3] = { m.emissive[0], m.emissive[1], m.emissive[2] };
  if (m.useNodes && !m.graph.nodes.empty()) {
    std::string bakedTex;
    if (EvaluateMaterialGraph(m.graph, nodeAlbedo, nodeRough, nodeMetal, nodeEmissive, bakedTex)) {
      if (!bakedTex.empty()) {
        nodeAlbedoTex = bakedTex;
      }
    }
  }
  partMat.albedo[0] = nodeAlbedo[0];
  partMat.albedo[1] = nodeAlbedo[1];
  partMat.albedo[2] = nodeAlbedo[2];
  partMat.alpha = m.alpha;
  partMat.roughness = nodeRough < 0.0f ? 0.0f : (nodeRough > 1.0f ? 1.0f : nodeRough);
  partMat.metallic = nodeMetal < 0.0f ? 0.0f : (nodeMetal > 1.0f ? 1.0f : nodeMetal);
  partMat.emissive[0] = nodeEmissive[0];
  partMat.emissive[1] = nodeEmissive[1];
  partMat.emissive[2] = nodeEmissive[2];
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
  partMat.detailTiling = m.detailTiling;
  partMat.layerWeight = m.layerWeight;
  partMat.layerAlbedo[0] = m.layerAlbedo[0];
  partMat.layerAlbedo[1] = m.layerAlbedo[1];
  partMat.layerAlbedo[2] = m.layerAlbedo[2];
  partMat.layerRoughness = m.layerRoughness;
  partMat.layerMetallic = m.layerMetallic;
  partMat.layerHeight = m.layerHeight;
  partMat.layerHeightBlend = m.layerHeightBlend;
  partMat.sssColor[0] = m.sssColor[0];
  partMat.sssColor[1] = m.sssColor[1];
  partMat.sssColor[2] = m.sssColor[2];
  partMat.sssIntensity = m.sssIntensity;
  partMat.sssKind = m.sssKind < 0 ? 0 : (m.sssKind > 3 ? 3 : m.sssKind);
  partMat.triplanar = m.triplanar;
  partMat.triplanarScale = m.triplanarScale < 0.001f ? 0.001f : m.triplanarScale;
  uint32_t flags = 0;
  flags |= ResolveMaterialSlot(assets, nodeAlbedoTex, 1u, partMat.albedoTex);
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
  flags |= ResolveMaterialSlot(assets, m.detailAlbedoTex, 1048576u, partMat.detailAlbedoTex);
  if (!m.detailNormalTex.empty()) {
    assets.EnsureTextureKind(m.detailNormalTex, TexKind::Normal);
    flags |= ResolveMaterialSlot(assets, m.detailNormalTex, 2097152u, partMat.detailNormalTex);
  }
  if (m.layerWeight > 0.001f) {
    flags |= 4194304u;
  }
  if (m.layerHeightBlend) {
    flags |= 8388608u;
  }
  if (m.hasSpecular) {
    flags |= 65536u;
  }
  if (m.sssKind == 1) {
    flags |= 16777216u;
  } else if (m.sssKind == 2) {
    flags |= 33554432u;
  } else if (m.sssKind == 3) {
    flags |= 67108864u;
  }
  if (m.triplanar) {
    flags |= 536870912u;
  }
  partMat.vtTable = 0;
  partMat.vtTail = 0;
  partMat.vtResidentMip = 99.0f;
  partMat.vtGuid.clear();
  if (m.vtAlbedo && !nodeAlbedoTex.empty()) {
    VtBundle bundle;
    const TextureRecord* vtRec = assets.GetTexByGuid(nodeAlbedoTex);
    const TextureAssetData* vtData = (vtRec != nullptr && vtRec->loaded) ? &vtRec->data : nullptr;
    if (assets.GetVirtualBundle(nodeAlbedoTex, bundle) && bundle.valid) {
      flags |= 134217728u;
      partMat.albedoTex = bundle.physical;
      partMat.vtTable = bundle.table;
      partMat.vtTail = bundle.tail;
      partMat.vtResidentMip = bundle.residentMip;
      partMat.vtGuid = nodeAlbedoTex;
    } else if (vtData != nullptr && assets.CreateVirtualTexture(vtData, nodeAlbedoTex) >= 0 && assets.GetVirtualBundle(nodeAlbedoTex, bundle) && bundle.valid) {
      flags |= 134217728u;
      partMat.albedoTex = bundle.physical;
      partMat.vtTable = bundle.table;
      partMat.vtTail = bundle.tail;
      partMat.vtResidentMip = bundle.residentMip;
      partMat.vtGuid = nodeAlbedoTex;
    }
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
    ImGui_ImplDX11_InvalidateDeviceObjects();
    rhi->Resize(cw, ch);
    ImGui_ImplDX11_CreateDeviceObjects();
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
  {
    float rcx = 0.0f;
    float rcy = 0.0f;
    float rcz = 0.0f;
    camera.GetPosition(rcx, rcy, rcz);
    if (scene.Origin().RebaseIfNeeded(rcx, rcy, rcz, 5000.0)) {
      double sx = 0.0;
      double sy = 0.0;
      double sz = 0.0;
      scene.Origin().GetLastShift(sx, sy, sz);
      scene.RebaseWorld(sx, sy, sz);
      camera.SetPosition(rcx - static_cast<float>(sx), rcy - static_cast<float>(sy), rcz - static_cast<float>(sz));
      log.Add(LogLevel::Info, "World rebased for large coordinates");
    }
  }
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
  renderer.SetShadowFilter(viewShadowFilter);
  renderer.SetNormalBias(viewNormalBias);
  renderer.SetAutoBias(viewAutoBias);
  renderer.SetGrade(viewExposure, viewToneMap == 0);
  renderer.SetToneMapping(viewToneMap);
  renderer.SetAutoExposure(viewAutoExp);
  renderer.SetRobustExposure(viewRobustExp);
  renderer.SetGrading(viewContrast, viewSaturation, viewTemp, viewTint);
  renderer.SetLutAmount(viewLutAmount);
  renderer.SetBloom(viewBloom);
  renderer.SetSsao(viewSsao, viewSsaoIntensity, viewSsaoRadius);
  renderer.SetAoMode(viewSsao ? viewAoMode : 0);
  renderer.SetAttenuationMode(viewAttenMode);
  renderer.SetPhysicalScale(viewPhysScale);
  renderer.SetCameraLightMask(static_cast<uint32_t>(viewLightLayers));
  renderer.SetSpotCookieOn(viewSpotCookie);
  renderer.SetSsr(viewSsr);
  renderer.SetSsrTemporal(viewSsrTemporal, viewSsrTemporalBlend);
  renderer.SetContact(viewContact);
  renderer.SetDof(viewDof, viewFocusDist, viewFocusRange);
  renderer.SetMotionBlur(viewMotion, viewMotionScale);
  renderer.SetFilm(viewCa, viewVig, viewGrain);
  renderer.SetHeatHaze(viewHeat);
  renderer.SetLensDirt(viewLensDirt);
  renderer.SetSsgi(viewSsgi);
  renderer.SetFxaa(viewFxaa);
  renderer.SetTaa(viewTaa);
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
    if (renderer.ProbeRefreshNeeded()) {
      EntityId best = EntityId::Invalid();
      float bestD = 0.0f;
      std::vector<EntityId> allIds = scene.All();
      for (size_t i = 0; i < allIds.size(); ++i) {
        const Entity* pe = scene.Get(allIds[i]);
        if (pe == nullptr || !pe->hasProbe) {
          continue;
        }
        float dx = pe->transform.position[0] - cpos[0];
        float dy = pe->transform.position[1] - cpos[1];
        float dz = pe->transform.position[2] - cpos[2];
        float dd = dx * dx + dy * dy + dz * dz;
        if (!best.IsValid() || dd < bestD) {
          best = allIds[i];
          bestD = dd;
        }
      }
      if (best.IsValid()) {
        BakeProbeEntity(best);
      }
      renderer.MarkProbesRefreshed();
    }
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
  renderer.SetHeightFog(viewFogHeight, viewFogBase);
  renderer.SetGodRays(viewGodRays, viewGodDensity, viewGodDecay, viewGodIntensity);
  renderer.SetEnvIntensity(viewEnv);
  SubmitSceneDraws(cpos);
  renderer.EndObjectsToTexture(cpos);
  rhi->Clear(0.03f, 0.03f, 0.04f, 1.0f);
}
static uint64_t StaticMixHash(uint64_t h, const void* data, size_t bytes) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
  for (size_t i = 0; i < bytes; ++i) {
    h ^= static_cast<uint64_t>(p[i]);
    h *= 1099511628211ull;
  }
  return h;
}
static uint64_t StaticMatSig(const DeferredMaterial& m) {
  uint64_t h = 1469598103934665603ull;
  h = StaticMixHash(h, &m.texFlags, sizeof(m.texFlags));
  h = StaticMixHash(h, m.albedo, sizeof(float) * 3);
  h = StaticMixHash(h, &m.alpha, sizeof(m.alpha));
  h = StaticMixHash(h, &m.roughness, sizeof(m.roughness));
  h = StaticMixHash(h, &m.metallic, sizeof(m.metallic));
  h = StaticMixHash(h, m.emissive, sizeof(float) * 3);
  h = StaticMixHash(h, &m.emissiveStrength, sizeof(m.emissiveStrength));
  h = StaticMixHash(h, &m.alphaMode, sizeof(m.alphaMode));
  h = StaticMixHash(h, &m.alphaCutoff, sizeof(m.alphaCutoff));
  h = StaticMixHash(h, &m.unlit, sizeof(m.unlit));
  h = StaticMixHash(h, &m.doubleSided, sizeof(m.doubleSided));
  const RHITexture slots[20] = { m.albedoTex, m.normalTex, m.ormTex, m.heightTex, m.lightmapTex, m.emissiveTex, m.specularTex, m.specularColorTex, m.clearcoatTex, m.clearcoatRoughnessTex, m.clearcoatNormalTex, m.sheenColorTex, m.sheenRoughnessTex, m.anisotropyTex, m.iridescenceTex, m.iridescenceThicknessTex, m.transmissionTex, m.thicknessTex, m.detailAlbedoTex, m.detailNormalTex };
  h = StaticMixHash(h, slots, sizeof(slots));
  return h == 0 ? 1 : h;
}
static void StaticHex16(uint64_t v, char out[17]) {
  static const char* digits = "0123456789ABCDEF";
  for (int i = 15; i >= 0; --i) {
    out[i] = digits[v & 15ull];
    v >>= 4;
  }
  out[16] = 0;
}
void EditorApp::DrawMeshRanges(const float world[16], const MeshRecord* rec, uint32_t partOff, uint32_t partCount, const DeferredMaterial* mat, float fade, const float* aabbMin, const float* aabbMax, const float* camPos) {
  if (!viewMeshletCull || rec == nullptr || rec->meshlets.size() < 2 || partCount == 0) {
    if (rec != nullptr) {
      renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, partOff, partCount, mat, fade, aabbMin, aabbMax);
    }
    return;
  }
  float planes[6][4];
  if (!renderer.GetFrustumPlanes(planes)) {
    renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, partOff, partCount, mat, fade, aabbMin, aabbMax);
    return;
  }
  uint32_t partEnd = partOff + partCount;
  uint32_t visCount = 0;
  uint32_t totalCount = 0;
  for (size_t r = 0; r < rec->meshlets.size(); ++r) {
    const MeshletRange& range = rec->meshlets[r];
    uint32_t s = partOff > range.indexOffset ? partOff : range.indexOffset;
    uint32_t re = range.indexOffset + range.indexCount;
    uint32_t e = partEnd < re ? partEnd : re;
    if (e <= s) {
      continue;
    }
    ++totalCount;
    if (MeshletRangeVisible(world, range, planes, camPos)) {
      ++visCount;
    }
  }
  if (visCount == totalCount) {
    renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, partOff, partCount, mat, fade, aabbMin, aabbMax);
    return;
  }
  for (size_t r = 0; r < rec->meshlets.size(); ++r) {
    const MeshletRange& range = rec->meshlets[r];
    uint32_t s = partOff > range.indexOffset ? partOff : range.indexOffset;
    uint32_t re = range.indexOffset + range.indexCount;
    uint32_t e = partEnd < re ? partEnd : re;
    if (e <= s) {
      continue;
    }
    if (!MeshletRangeVisible(world, range, planes, camPos)) {
      continue;
    }
    renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, s, e - s, mat, fade, aabbMin, aabbMax);
  }
}
std::unordered_set<EntityId, EntityIdHash> EditorApp::SubmitStaticBatches(const float cpos[3]) {
  (void)cpos;
  std::unordered_set<EntityId, EntityIdHash> done;
  if (!viewStaticBatch) {
    if (!lastStaticKey.empty()) {
      assets.DiscardBatchMeshes();
      lastStaticKey.clear();
      lastStaticBuckets.clear();
    }
    renderer.SetStaticBatchInfo(0, 0);
    return done;
  }
  struct Entry {
    EntityId id;
    float world[16];
    const MeshAssetData* data;
    uint32_t off;
    uint32_t count;
    DeferredMaterial mat;
    uint64_t sig;
  };
  std::map<uint64_t, std::vector<Entry>> buckets;
  std::map<uint64_t, DeferredMaterial> bucketMats;
  uint64_t keyH = 1469598103934665603ull;
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr || !e->isStatic || !e->hasMesh || e->meshGuid.empty() || e->hasLightmap || e->hasLod) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec == nullptr || !rec->loaded) {
      continue;
    }
    keyH = StaticMixHash(keyH, &e->id.index, sizeof(e->id.index));
    keyH = StaticMixHash(keyH, &e->id.generation, sizeof(e->id.generation));
    keyH = StaticMixHash(keyH, world, sizeof(world));
    keyH = StaticMixHash(keyH, e->meshGuid.data(), e->meshGuid.size());
    if (rec->data.parts.empty()) {
      Entry en;
      en.id = e->id;
      std::memcpy(en.world, world, sizeof(en.world));
      en.data = &rec->data;
      en.off = 0;
      en.count = static_cast<uint32_t>(rec->data.indices.size());
      en.mat = renderer.DefaultMaterial();
      en.sig = StaticMatSig(en.mat) ^ 0x9E3779B97F4A7C15ull;
      keyH = StaticMixHash(keyH, &en.off, sizeof(en.off));
      keyH = StaticMixHash(keyH, &en.count, sizeof(en.count));
      keyH = StaticMixHash(keyH, &en.sig, sizeof(en.sig));
      buckets[en.sig].push_back(en);
      if (bucketMats.find(en.sig) == bucketMats.end()) {
        bucketMats[en.sig] = en.mat;
      }
    } else {
      for (size_t p = 0; p < rec->data.parts.size(); ++p) {
        const MeshPartData& part = rec->data.parts[p];
        if (part.indexCount == 0 || static_cast<size_t>(part.indexOffset) + static_cast<size_t>(part.indexCount) > rec->data.indices.size()) {
          continue;
        }
        DeferredMaterial partMat;
        const DeferredMaterial* useMat = nullptr;
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
          FillDeferredFromMesh(assets, rec->data.materials[matIdx], partMat);
          useMat = &partMat;
        }
        if (useMat == nullptr || useMat->transmission > 0.0f || useMat->alphaMode == 2) {
          continue;
        }
        Entry en;
        en.id = e->id;
        std::memcpy(en.world, world, sizeof(en.world));
        en.data = &rec->data;
        en.off = part.indexOffset;
        en.count = part.indexCount;
        en.mat = *useMat;
        en.sig = StaticMatSig(en.mat);
        keyH = StaticMixHash(keyH, &en.off, sizeof(en.off));
        keyH = StaticMixHash(keyH, &en.count, sizeof(en.count));
        keyH = StaticMixHash(keyH, &en.sig, sizeof(en.sig));
        buckets[en.sig].push_back(en);
        if (bucketMats.find(en.sig) == bucketMats.end()) {
          bucketMats[en.sig] = en.mat;
        }
      }
    }
  }
  char keyBuf[17];
  StaticHex16(keyH == 0 ? 1 : keyH, keyBuf);
  std::string key = keyBuf;
  if (key != lastStaticKey) {
    assets.DiscardBatchMeshes();
    lastStaticBuckets.clear();
    for (std::map<uint64_t, std::vector<Entry>>::iterator bi = buckets.begin(); bi != buckets.end(); ++bi) {
      if (bi->second.size() < 2) {
        continue;
      }
      std::vector<StaticMergePart> refs;
      for (size_t r = 0; r < bi->second.size(); ++r) {
        StaticMergePart ref;
        ref.data = bi->second[r].data;
        ref.indexOffset = bi->second[r].off;
        ref.indexCount = bi->second[r].count;
        std::memcpy(ref.world, bi->second[r].world, sizeof(ref.world));
        refs.push_back(ref);
      }
      MeshAssetData merged;
      if (!BuildMergedMesh(refs, merged)) {
        continue;
      }
      char gb[32];
      gb[0] = 'b';
      gb[1] = 'a';
      gb[2] = 't';
      gb[3] = 'c';
      gb[4] = 'h';
      gb[5] = ':';
      gb[6] = '/';
      gb[7] = '/';
      StaticHex16(bi->first, gb + 8);
      gb[24] = 0;
      assets.UpsertBatchMesh(gb, merged);
      if (!assets.EnsureMeshGpu(gb, 4194304)) {
        continue;
      }
      const MeshRecord* mr = assets.GetByGuid(gb);
      if (mr == nullptr || !mr->gpuReady) {
        continue;
      }
      StaticBucketDraw bd;
      bd.guid = gb;
      bd.mat = bucketMats[bi->first];
      bd.bmin[0] = merged.aabbMin[0];
      bd.bmin[1] = merged.aabbMin[1];
      bd.bmin[2] = merged.aabbMin[2];
      bd.bmax[0] = merged.aabbMax[0];
      bd.bmax[1] = merged.aabbMax[1];
      bd.bmax[2] = merged.aabbMax[2];
      for (size_t r = 0; r < bi->second.size(); ++r) {
        bd.ids.push_back(bi->second[r].id);
      }
      lastStaticBuckets.push_back(bd);
    }
    lastStaticKey = key;
  }
  float ident[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  uint32_t drawn = 0;
  uint32_t saved = 0;
  for (size_t b = 0; b < lastStaticBuckets.size(); ++b) {
    const StaticBucketDraw& bd = lastStaticBuckets[b];
    const MeshRecord* mr = assets.GetByGuid(bd.guid);
    if (mr == nullptr || !mr->gpuReady) {
      continue;
    }
    if (!bd.mat.vtGuid.empty()) {
      float bdx = (bd.bmin[0] + bd.bmax[0]) * 0.5f - cpos[0];
      float bdy = (bd.bmin[1] + bd.bmax[1]) * 0.5f - cpos[1];
      float bdz = (bd.bmin[2] + bd.bmax[2]) * 0.5f - cpos[2];
      assets.UpdateVirtualTexture(bd.mat.vtGuid, sqrtf(bdx * bdx + bdy * bdy + bdz * bdz));
    }
    renderer.DrawObjectEx(ident, mr->gpuVB, mr->gpuIB, 0, mr->gpuCount, &bd.mat, 0.0f, bd.bmin, bd.bmax);
    for (size_t r = 0; r < bd.ids.size(); ++r) {
      done.insert(bd.ids[r]);
    }
    ++drawn;
    saved += static_cast<uint32_t>(bd.ids.size()) - 1;
  }
  renderer.SetStaticBatchInfo(drawn, saved);
  return done;
}
static void EulerDegToQuat(const float euler[3], float quat[4]) {
  Transform t;
  MakeIdentityTransform(t);
  t.rotation[0] = euler[0];
  t.rotation[1] = euler[1];
  t.rotation[2] = euler[2];
  float m[16];
  ComposeMatrix(t, m);
  float trace = m[0] + m[5] + m[10];
  if (trace > 0.0f) {
    float s = std::sqrt(trace + 1.0f) * 2.0f;
    quat[3] = 0.25f * s;
    quat[0] = (m[9] - m[6]) / s;
    quat[1] = (m[2] - m[8]) / s;
    quat[2] = (m[4] - m[1]) / s;
  } else if (m[0] > m[5] && m[0] > m[10]) {
    float s = std::sqrt(1.0f + m[0] - m[5] - m[10]) * 2.0f;
    quat[3] = (m[9] - m[6]) / s;
    quat[0] = 0.25f * s;
    quat[1] = (m[1] + m[4]) / s;
    quat[2] = (m[2] + m[8]) / s;
  } else if (m[5] > m[10]) {
    float s = std::sqrt(1.0f + m[5] - m[0] - m[10]) * 2.0f;
    quat[3] = (m[2] - m[8]) / s;
    quat[0] = (m[1] + m[4]) / s;
    quat[1] = 0.25f * s;
    quat[2] = (m[6] + m[9]) / s;
  } else {
    float s = std::sqrt(1.0f + m[10] - m[0] - m[5]) * 2.0f;
    quat[3] = (m[4] - m[1]) / s;
    quat[0] = (m[2] + m[8]) / s;
    quat[1] = (m[6] + m[9]) / s;
    quat[2] = 0.25f * s;
  }
  float l = std::sqrt(quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3]);
  if (l > 1e-9f) {
    quat[0] /= l;
    quat[1] /= l;
    quat[2] /= l;
    quat[3] /= l;
  }
}
static void ComposeMatrixPQ(const float pos[3], const float quat[4], const float scale[3], float m[16]) {
  float x = quat[0];
  float y = quat[1];
  float z = quat[2];
  float w = quat[3];
  float l = std::sqrt(x * x + y * y + z * z + w * w);
  if (l < 1e-9f) {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 1.0f;
    l = 1.0f;
  }
  x /= l;
  y /= l;
  z /= l;
  w /= l;
  float xx = x * x;
  float yy = y * y;
  float zz = z * z;
  float xy = x * y;
  float xz = x * z;
  float yz = y * z;
  float wx = w * x;
  float wy = w * y;
  float wz = w * z;
  m[0] = (1.0f - 2.0f * (yy + zz)) * scale[0];
  m[1] = 2.0f * (xy + wz) * scale[0];
  m[2] = 2.0f * (xz - wy) * scale[0];
  m[3] = 0.0f;
  m[4] = 2.0f * (xy - wz) * scale[1];
  m[5] = (1.0f - 2.0f * (xx + zz)) * scale[1];
  m[6] = 2.0f * (yz + wx) * scale[1];
  m[7] = 0.0f;
  m[8] = 2.0f * (xz + wy) * scale[2];
  m[9] = 2.0f * (yz - wx) * scale[2];
  m[10] = (1.0f - 2.0f * (xx + yy)) * scale[2];
  m[11] = 0.0f;
  m[12] = pos[0];
  m[13] = pos[1];
  m[14] = pos[2];
  m[15] = 1.0f;
}
static uint64_t RigidbodyDescHash(const RigidbodyData& r, const std::string& meshGuid) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&h](const void* data, size_t bytes) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
      h ^= static_cast<uint64_t>(p[i]);
      h *= 1099511628211ull;
    }
  };
  mix(&r.motionType, sizeof(r.motionType));
  mix(&r.mass, sizeof(r.mass));
  mix(&r.colliderType, sizeof(r.colliderType));
  mix(r.extents, sizeof(r.extents));
  mix(&r.friction, sizeof(r.friction));
  mix(&r.restitution, sizeof(r.restitution));
  mix(meshGuid.data(), meshGuid.size());
  return h == 0 ? 1 : h;
}
void EditorApp::SyncAudio(float dt) {
  (void)dt;
  if (!audioReady) {
    if (!audio.Initialize(48000, 2)) {
      return;
    }
    audioReady = true;
  }
  float camX = 0.0f;
  float camY = 0.0f;
  float camZ = 0.0f;
  camera.GetPosition(camX, camY, camZ);
  float yaw = 0.0f;
  float pitch = 0.0f;
  camera.GetYawPitch(yaw, pitch);
  const float listenerPos[3] = { camX, camY, camZ };
  const float listenerFwd[3] = { std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
  const float listenerUp[3] = { 0.0f, 1.0f, 0.0f };
  audio.SetListener(listenerPos, listenerFwd, listenerUp);
  const std::string assetDir = ResolveAssetsDir();
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    Entity* e = scene.Get(all[i]);
    if (e == nullptr || !e->hasAudioSource) {
      continue;
    }
    const AudioSourceData& a = e->audioSource;
    auto it = audioSources.find(e->id);
    if (it == audioSources.end()) {
      if (a.clipPath.empty()) {
        continue;
      }
      uint32_t clipId = 0xFFFFFFFFu;
      auto cached = audioClipCache.find(a.clipPath);
      if (cached != audioClipCache.end()) {
        clipId = cached->second;
      } else {
        std::string full = a.clipPath;
        if (!assetDir.empty() && !std::filesystem::path(a.clipPath).is_absolute()) {
          full = assetDir + "/" + a.clipPath;
        }
        uint32_t loaded = 0xFFFFFFFFu;
        if (audio.LoadClip(full, loaded)) {
          audioClipCache[a.clipPath] = loaded;
          clipId = loaded;
        }
      }
      if (clipId == 0xFFFFFFFFu) {
        continue;
      }
      const uint32_t sourceId = audio.AddSource(clipId, static_cast<uint32_t>(a.bus < 0 ? 0 : a.bus));
      if (sourceId == 0) {
        continue;
      }
      AudioEntry entry;
      entry.sourceId = sourceId;
      entry.clipId = clipId;
      entry.clipPath = a.clipPath;
      entry.volume = a.volume;
      entry.playing = a.playing;
      entry.spatial = a.spatial;
      entry.loop = a.loop;
      entry.bus = a.bus;
      audioSources[e->id] = entry;
      it = audioSources.find(e->id);
    }
    AudioEntry& entry = it->second;
    if (entry.volume != a.volume) {
      audio.SetSourceGain(entry.sourceId, a.volume);
      entry.volume = a.volume;
    }
    if (entry.playing != a.playing) {
      audio.SetSourcePlaying(entry.sourceId, a.playing);
      entry.playing = a.playing;
    }
    if (entry.spatial != a.spatial) {
      audio.SetSourceSpatial(entry.sourceId, a.spatial);
      entry.spatial = a.spatial;
    }
    if (entry.loop != a.loop) {
      audio.SetSourceLoop(entry.sourceId, a.loop);
      entry.loop = a.loop;
    }
    if (entry.bus != a.bus) {
      audio.SetSourceBus(entry.sourceId, static_cast<uint32_t>(a.bus < 0 ? 0 : a.bus));
      entry.bus = a.bus;
    }
    if (a.spatial) {
      audio.SetSourcePosition(entry.sourceId, e->transform.position);
    }
  }
  for (auto it = audioSources.begin(); it != audioSources.end();) {
    Entity* e = scene.Get(it->first);
    if (e == nullptr || !e->hasAudioSource) {
      audio.RemoveSource(it->second.sourceId);
      it = audioSources.erase(it);
    } else {
      ++it;
    }
  }
  audio.Update();
}
void EditorApp::PropertyLabel(const char* label) {
  const float start = ImGui::GetCursorPosX();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(start + 140.0f);
  ImGui::SetNextItemWidth(-1.0f);
}
void EditorApp::PropertySection(const char* title) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.64f, 1.0f, 1.0f));
  ImGui::TextUnformatted(title);
  ImGui::PopStyleColor();
}
void EditorApp::DrawAudioSourceSection(Entity* e) {
  if (!e->hasAudioSource) {
    return;
  }
  PropertySection("Audio Source");
  ImGui::SameLine();
  if (ImGui::SmallButton("Remove##audiosource")) {
    std::unique_ptr<Command> cmd(new RemoveAudioSourceCmd(e->id));
    undo.Execute(std::move(cmd), scene);
    return;
  }
  ImGui::PushID("AudioSource");
  AudioSourceData as = e->audioSource;
  bool changed = false;
  char clipBuf[256];
  std::snprintf(clipBuf, sizeof(clipBuf), "%s", as.clipPath.c_str());
  PropertyLabel("Clip");
  ImGui::SetNextItemWidth(-70.0f);
  if (ImGui::InputText("##Clip", clipBuf, sizeof(clipBuf))) {
    as.clipPath = clipBuf;
    changed = true;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60.0f);
  if (ImGui::Button("Browse##clip")) {
    std::string picked;
    if (ShowOpenAudioDialog(window.NativeHandle(), picked)) {
      std::error_code cec;
      const std::string assetDir = ResolveAssetsDir();
      std::filesystem::create_directories(std::filesystem::path(assetDir) / "Audio", cec);
      const std::string rel = "Audio/" + std::filesystem::path(picked).filename().string();
      const std::string dst = (std::filesystem::path(assetDir) / rel).string();
      std::filesystem::copy_file(picked, dst, cec);
      if (cec) {
        Announce(LogLevel::Error, std::string("Could not copy audio clip: ") + picked);
      } else {
        as.clipPath = rel;
        changed = true;
      }
    }
  }
  PropertyLabel("Volume");
  if (ImGui::DragFloat("##Volume", &as.volume, 0.01f, 0.0f, 4.0f)) {
    changed = true;
  }
  PropertyLabel("Spatial");
  if (ImGui::Checkbox("##Spatial", &as.spatial)) {
    changed = true;
  }
  if (as.spatial) {
    PropertyLabel("Min Distance");
    if (ImGui::DragFloat("##Min Distance", &as.minDistance, 0.1f, 0.01f, 1000.0f)) {
      changed = true;
    }
    PropertyLabel("Max Distance");
    if (ImGui::DragFloat("##Max Distance", &as.maxDistance, 0.1f, 0.01f, 10000.0f)) {
      changed = true;
    }
    int model = as.rolloffModel < 0 ? 0 : (as.rolloffModel > 2 ? 2 : as.rolloffModel);
    PropertyLabel("Rolloff");
    if (ImGui::Combo("##Rolloff", &model, "Linear\0Inverse\0Exponential\0")) {
      as.rolloffModel = model;
      changed = true;
    }
    PropertyLabel("Rolloff Factor");
    if (ImGui::DragFloat("##Rolloff Factor", &as.rolloff, 0.05f, 0.01f, 16.0f)) {
      changed = true;
    }
  }
  PropertyLabel("Loop");
  if (ImGui::Checkbox("##Loop", &as.loop)) {
    changed = true;
  }
  PropertyLabel("Playing");
  if (ImGui::Checkbox("##Playing", &as.playing)) {
    changed = true;
  }
  const size_t busCount = audio.Mixer().BusCount();
  if (busCount > 0) {
    std::string names;
    for (size_t i = 0; i < busCount; ++i) {
      const AudioBusDesc* b = audio.Mixer().GetBus(static_cast<uint32_t>(i));
      if (b == nullptr) {
        continue;
      }
      if (!names.empty()) {
        names.push_back('\0');
      }
      names += b->name;
    }
    int bus = as.bus < 0 ? 0 : (static_cast<size_t>(as.bus) < busCount ? as.bus : 0);
    PropertyLabel("Bus");
    if (ImGui::Combo("##Bus", &bus, names.c_str())) {
      as.bus = bus;
      changed = true;
    }
  }
  if (changed) {
    edits.PushAudioSource(e->id, as);
  }
  ImGui::PopID();
}
void EditorApp::DrawAudioMixer() {
  if (!ImGui::Begin("Audio Mixer", &showAudioMixer)) {
    ImGui::End();
    return;
  }
  if (!audioReady) {
    ImGui::TextDisabled("Audio engine not initialized");
    ImGui::End();
    return;
  }
  AudioMixer& mixer = audio.Mixer();
  ImGui::Text("Buses");
  ImGui::Separator();
  const size_t busCount = mixer.BusCount();
  for (size_t i = 0; i < busCount; ++i) {
    AudioBusDesc* b = mixer.GetBusMutable(static_cast<uint32_t>(i));
    if (b == nullptr) {
      continue;
    }
    ImGui::PushID(static_cast<int>(i));
    ImGui::Text("%s", b->name.c_str());
    ImGui::SameLine();
    if (i == 0) {
      ImGui::TextDisabled("(master)");
    }
    float vol = b->volume;
    if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.5f)) {
      b->volume = vol;
    }
    bool muted = b->muted;
    if (ImGui::Checkbox("Mute", &muted)) {
      b->muted = muted;
    }
    ImGui::SameLine();
    bool soloed = b->soloed;
    if (ImGui::Checkbox("Solo", &soloed)) {
      b->soloed = soloed;
    }
    ImGui::SameLine();
    bool reverb = b->reverbEnabled;
    if (ImGui::Checkbox("Reverb", &reverb)) {
      b->reverbEnabled = reverb;
    }
    if (b->reverbEnabled) {
      float wet = b->reverbWet;
      if (ImGui::SliderFloat("Reverb Wet", &wet, 0.0f, 1.0f)) {
        b->reverbWet = wet;
      }
      float room = b->reverbRoomSize;
      if (ImGui::SliderFloat("Reverb Room", &room, 0.0f, 1.0f)) {
        b->reverbRoomSize = room;
      }
      float damp = b->reverbDamping;
      if (ImGui::SliderFloat("Reverb Damping", &damp, 0.0f, 1.0f)) {
        b->reverbDamping = damp;
      }
    }
    if (b->ducking.size() > 0) {
      ImGui::Text("Ducking");
      for (size_t d = 0; d < b->ducking.size(); ++d) {
        AudioDuckLink& link = b->ducking[d];
        const AudioBusDesc* target = link.targetBus >= 0 ? mixer.GetBus(static_cast<uint32_t>(link.targetBus)) : nullptr;
        ImGui::Text("  -> %s", target != nullptr ? target->name.c_str() : "(none)");
        ImGui::SameLine();
        float db = link.amountDb;
        if (ImGui::SliderFloat("dB", &db, -40.0f, 0.0f)) {
          link.amountDb = db;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
          b->ducking.erase(b->ducking.begin() + static_cast<long>(d));
          break;
        }
      }
    }
    if (i != 0) {
      ImGui::SameLine();
      if (ImGui::SmallButton("Duck Target")) {
        AudioDuckLink link;
        link.targetBus = 0;
        link.amountDb = -6.0f;
        b->ducking.push_back(link);
      }
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::Text("Voices: %u", audio.ActiveVoiceCount());
  ImGui::SameLine();
  ImGui::Text("Peak: %.3f", audio.MasterPeak());
  ImGui::Separator();
  if (ImGui::Button("Save Mixer")) {
    const std::string path = AudioMixerPath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (audio.Mixer().SaveToFile(path)) {
      Announce(LogLevel::Success, "Audio mixer saved to " + path);
    } else {
      Announce(LogLevel::Error, "Could not save audio mixer");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Mixer")) {
    const std::string path = AudioMixerPath();
    if (audio.Mixer().LoadFromFile(path)) {
      audio.Update();
      Announce(LogLevel::Success, "Audio mixer loaded");
    } else {
      Announce(LogLevel::Warning, "No audio mixer found at " + path);
    }
  }
  ImGui::End();
}
void EditorApp::SyncPhysics(float dt) {
  if (!viewSimulate) {
    return;
  }
  if (!physReady) {
    if (!physJobs.Initialize()) {
      viewSimulate = false;
      return;
    }
    if (!physWorld.Initialize(&physJobs)) {
      viewSimulate = false;
      return;
    }
    physReady = true;
  }
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    Entity* e = scene.Get(all[i]);
    if (e == nullptr || !e->hasRigidbody) {
      continue;
    }
    float equat[4];
    EulerDegToQuat(e->transform.rotation, equat);
    uint64_t h = RigidbodyDescHash(e->rigidbody, e->meshGuid);
    auto it = physBodies.find(e->id);
    bool recreate = it == physBodies.end() || it->second.descHash != h || !physWorld.HasBody(it->second.body);
    if (recreate) {
      if (it != physBodies.end() && physWorld.HasBody(it->second.body)) {
        physWorld.DestroyBody(it->second.body);
      }
      PhysBodyDesc desc;
      desc.motion = e->rigidbody.motionType == 0 ? PhysMotionType::Static : (e->rigidbody.motionType == 1 ? PhysMotionType::Kinematic : PhysMotionType::Dynamic);
      desc.mass = e->rigidbody.mass;
      desc.collider = e->rigidbody.colliderType == 1 ? PhysColliderType::Sphere : (e->rigidbody.colliderType == 2 ? PhysColliderType::Capsule : (e->rigidbody.colliderType == 3 ? PhysColliderType::Mesh : PhysColliderType::Box));
      desc.extents[0] = e->rigidbody.extents[0];
      desc.extents[1] = e->rigidbody.extents[1];
      desc.extents[2] = e->rigidbody.extents[2];
      const MeshRecord* mrec = nullptr;
      if (e->rigidbody.colliderType == 3 && e->hasMesh && !e->meshGuid.empty()) {
        mrec = assets.GetByGuid(e->meshGuid);
        if (mrec != nullptr && mrec->loaded) {
          desc.meshVerts = mrec->data.positions.data();
          desc.meshVertCount = mrec->data.positions.size() / 3;
          desc.meshIndices = mrec->data.indices.data();
          desc.meshIndexCount = mrec->data.indices.size();
        } else {
          continue;
        }
      }
      desc.friction = e->rigidbody.friction;
      desc.restitution = e->rigidbody.restitution;
      uint32_t b = physWorld.CreateBody(desc, e->transform.position, equat);
      if (b == 0xFFFFFFFFu) {
        continue;
      }
      PhysEntry entry;
      entry.body = b;
      entry.descHash = h;
      entry.lastPos[0] = e->transform.position[0];
      entry.lastPos[1] = e->transform.position[1];
      entry.lastPos[2] = e->transform.position[2];
      entry.lastRot[0] = equat[0];
      entry.lastRot[1] = equat[1];
      entry.lastRot[2] = equat[2];
      entry.lastRot[3] = equat[3];
      entry.hasLast = true;
      physBodies[e->id] = entry;
    } else {
      PhysEntry& entry = it->second;
      bool moved = !entry.hasLast || entry.lastPos[0] != e->transform.position[0] || entry.lastPos[1] != e->transform.position[1] || entry.lastPos[2] != e->transform.position[2] || entry.lastRot[0] != equat[0] || entry.lastRot[1] != equat[1] || entry.lastRot[2] != equat[2] || entry.lastRot[3] != equat[3];
      if (moved && e->rigidbody.motionType != 2) {
        physWorld.TeleportBody(entry.body, e->transform.position, equat);
      }
      if (e->rigidbody.motionType == 1) {
        physWorld.SetKinematicTarget(entry.body, e->transform.position, equat);
      }
      entry.lastPos[0] = e->transform.position[0];
      entry.lastPos[1] = e->transform.position[1];
      entry.lastPos[2] = e->transform.position[2];
      entry.lastRot[0] = equat[0];
      entry.lastRot[1] = equat[1];
      entry.lastRot[2] = equat[2];
      entry.lastRot[3] = equat[3];
      entry.hasLast = true;
    }
  }
  for (auto it = physBodies.begin(); it != physBodies.end();) {
    Entity* e = scene.Get(it->first);
    if (e == nullptr || !e->hasRigidbody) {
      physWorld.DestroyBody(it->second.body);
      it = physBodies.erase(it);
    } else {
      ++it;
    }
  }
  physWorld.Step(dt);
}
void EditorApp::SubmitSceneDraws(const float cpos[3]) {
  SyncPhysics(frameDt);
  SyncAudio(frameDt);
  renderer.ClearLights();
  renderer.SetFallbackLight(!soloLightId.IsValid());
  renderer.ClearFogVolumes();
  {
    std::vector<EntityId> fogIds = scene.All();
    for (size_t i = 0; i < fogIds.size(); ++i) {
      const Entity* fe = scene.Get(fogIds[i]);
      if (fe == nullptr || !fe->hasFog) {
        continue;
      }
      float fbmin[3] = {
        fe->transform.position[0] - fe->fog.size[0] * 0.5f,
        fe->transform.position[1] - fe->fog.size[1] * 0.5f,
        fe->transform.position[2] - fe->fog.size[2] * 0.5f
      };
      float fbmax[3] = {
        fe->transform.position[0] + fe->fog.size[0] * 0.5f,
        fe->transform.position[1] + fe->fog.size[1] * 0.5f,
        fe->transform.position[2] + fe->fog.size[2] * 0.5f
      };
      renderer.AddFogVolume(fbmin, fbmax, fe->fog.density, fe->fog.color);
    }
  }
  {
    const PostData* active = nullptr;
    std::vector<EntityId> postIds = scene.All();
    for (size_t i = 0; i < postIds.size(); ++i) {
      const Entity* pe = scene.Get(postIds[i]);
      if (pe == nullptr || !pe->hasPost) {
        continue;
      }
      float hx = pe->post.size[0] * 0.5f;
      float hy = pe->post.size[1] * 0.5f;
      float hz = pe->post.size[2] * 0.5f;
      if (std::fabs(cpos[0] - pe->transform.position[0]) <= hx && std::fabs(cpos[1] - pe->transform.position[1]) <= hy && std::fabs(cpos[2] - pe->transform.position[2]) <= hz) {
        active = &pe->post;
        break;
      }
    }
    if (active != nullptr) {
      renderer.SetGrade(active->exposure, active->toneMap == 0);
      renderer.SetToneMapping(active->toneMap);
      renderer.SetGrading(active->contrast, active->saturation, viewTemp, viewTint);
      renderer.SetBloom(active->bloom);
    }
  }
  std::vector<EntityId> lightIds = scene.All();
  {
    size_t pointCount = 0;
    for (size_t i = 0; i < lightIds.size(); ++i) {
      const Entity* ce = scene.Get(lightIds[i]);
      if (ce != nullptr && ce->hasLight && ce->light.type == static_cast<int>(LightType::Point)) {
        ++pointCount;
      }
    }
    std::vector<const TextureAssetData*> cookieSlices(16, nullptr);
    std::string cookieKey = viewCookieGuid + "|";
    size_t spotOrd = 0;
    for (size_t i = 0; i < lightIds.size(); ++i) {
      const Entity* ce = scene.Get(lightIds[i]);
      if (ce == nullptr || !ce->hasLight || ce->light.type != static_cast<int>(LightType::Spot)) {
        continue;
      }
      size_t slice = pointCount + spotOrd;
      ++spotOrd;
      if (slice >= 16) {
        continue;
      }
      std::string cg = ce->light.cookieGuid.empty() ? viewCookieGuid : ce->light.cookieGuid;
      cookieKey += cg + "|";
      if (!cg.empty()) {
        const TextureRecord* ctr = assets.GetTexByGuid(cg);
        if (ctr != nullptr) {
          cookieKey += (ctr->loaded ? "1|" : "0|");
          cookieSlices[slice] = ctr->loaded ? &ctr->data : nullptr;
        }
      }
    }
    if (cookieKey != lastCookieKey) {
      lastCookieKey = cookieKey;
      RHITexture arr = assets.BuildCookieArray(cookieSlices);
      renderer.SetCookieArray(arr);
    }
  }
  for (size_t i = 0; i < lightIds.size(); ++i) {
    const Entity* e = scene.Get(lightIds[i]);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    if (soloLightId.IsValid() && !(e->id == soloLightId)) {
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
      pl.linkMask = static_cast<uint32_t>(e->light.linkMask);
      pl.layer = static_cast<uint32_t>(e->light.layer);
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
      sl.linkMask = static_cast<uint32_t>(e->light.linkMask);
      sl.layer = static_cast<uint32_t>(e->light.layer);
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
      dl.linkMask = static_cast<uint32_t>(e->light.linkMask);
      dl.layer = static_cast<uint32_t>(e->light.layer);
      renderer.AddDirectionalLight(dl);
    } else if (e->light.type == static_cast<int>(LightType::Rect)) {
      RenderRectLight rl;
      rl.pos[0] = e->transform.position[0];
      rl.pos[1] = e->transform.position[1];
      rl.pos[2] = e->transform.position[2];
      EntityForward(e->transform, rl.normal);
      rl.color[0] = e->light.color[0];
      rl.color[1] = e->light.color[1];
      rl.color[2] = e->light.color[2];
      rl.intensity = e->light.intensity;
      rl.halfW = e->light.areaW * 0.5f;
      rl.halfH = e->light.areaH * 0.5f;
      rl.range = e->light.radius;
      rl.linkMask = static_cast<uint32_t>(e->light.linkMask);
      rl.layer = static_cast<uint32_t>(e->light.layer);
      renderer.AddRectLight(rl);
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
  std::unordered_set<EntityId, EntityIdHash> batchedIds = SubmitStaticBatches(cpos);
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr) {
      continue;
    }
    if (batchedIds.find(e->id) != batchedIds.end()) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    if (viewSimulate && physReady && e->hasRigidbody) {
      auto pbit = physBodies.find(e->id);
      if (pbit != physBodies.end()) {
        float ppos[3] = { 0.0f, 0.0f, 0.0f };
        float prot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (physWorld.GetPose(pbit->second.body, ppos, prot)) {
          ComposeMatrixPQ(ppos, prot, e->transform.scale, world);
        }
      }
    }
    if (!e->hasMesh || e->meshGuid.empty()) {
      continue;
    }
    std::string drawGuids[2];
    float drawFades[2] = { 0.0f, 0.0f };
    int drawCount = 1;
    drawGuids[0] = e->meshGuid;
    drawGuids[1] = "";
    if (e->hasLod && e->lod.farGuid.empty() && e->lod.farDist > e->lod.nearDist) {
      const MeshRecord* baseRec = assets.GetByGuid(e->meshGuid);
      if (baseRec != nullptr) {
        float ldx = e->transform.position[0] - cpos[0];
        float ldy = e->transform.position[1] - cpos[1];
        float ldz = e->transform.position[2] - cpos[2];
        float ldist = sqrtf(ldx * ldx + ldy * ldy + ldz * ldz);
        int tierA = 0;
        int tierB = -1;
        float fadeB = 0.0f;
        float rdx = baseRec->data.aabbMax[0] - baseRec->data.aabbMin[0];
        float rdy = baseRec->data.aabbMax[1] - baseRec->data.aabbMin[1];
        float rdz = baseRec->data.aabbMax[2] - baseRec->data.aabbMin[2];
        float rradius = 0.5f * sqrtf(rdx * rdx + rdy * rdy + rdz * rdz);
        if (rradius > 0.001f && renderer.MainFovY() > 0.001f && renderer.RenderHeight() > 0) {
          Kizuri::SelectAutoLodCoverage(ldist, rradius, static_cast<float>(renderer.RenderHeight()), renderer.MainFovY(), tierA, tierB, fadeB);
        } else {
          Kizuri::SelectAutoLod(ldist, e->lod.nearDist, e->lod.farDist, tierA, tierB, fadeB);
        }
        std::string tierGuids[3];
        tierGuids[0] = e->meshGuid;
        tierGuids[1] = "";
        tierGuids[2] = "";
        for (int t = 1; t <= 2; ++t) {
          std::string sp = Kizuri::LodSiblingPath(baseRec->meshPath, t);
          if (!sp.empty()) {
            const MeshRecord* sr = assets.GetByMeshPath(sp);
            if (sr != nullptr) {
              tierGuids[t] = sr->guid;
            }
          }
        }
        std::string guidA = e->meshGuid;
        for (int t = tierA; t >= 0; --t) {
          if (!tierGuids[t].empty()) {
            guidA = tierGuids[t];
            break;
          }
        }
        drawGuids[0] = guidA;
        if (tierB >= 0) {
          std::string guidB = e->meshGuid;
          for (int t = tierB; t >= 0; --t) {
            if (!tierGuids[t].empty()) {
              guidB = tierGuids[t];
              break;
            }
          }
          drawGuids[1] = guidB;
          drawFades[0] = 1.0f - fadeB;
          drawFades[1] = fadeB;
          drawCount = 2;
        }
      }
    } else if (e->hasLod && !e->lod.farGuid.empty() && e->lod.farGuid != e->meshGuid && e->lod.farDist > e->lod.nearDist) {
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
              DrawMeshRanges(world, rec, 0, rec->gpuCount, nullptr, drawFades[li], rec->data.aabbMin, rec->data.aabbMax, cpos);
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
            if (useMat != nullptr && !useMat->vtGuid.empty()) {
              float vdx = world[12] - cpos[0];
              float vdy = world[13] - cpos[1];
              float vdz = world[14] - cpos[2];
              assets.UpdateVirtualTexture(useMat->vtGuid, sqrtf(vdx * vdx + vdy * vdy + vdz * vdz));
            }
            DrawMeshRanges(world, rec, part.indexOffset, part.indexCount, useMat, drawFades[li], rec->data.aabbMin, rec->data.aabbMax, cpos);
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
  {
    std::string decalKey;
    std::vector<const TextureAssetData*> decalTexs;
    lastDecalGuids.clear();
    for (size_t i = 0; i < ids.size(); ++i) {
      const Entity* ce = scene.Get(ids[i]);
      if (ce == nullptr || !ce->hasDecal || ce->decal.texGuid.empty()) {
        continue;
      }
      decalKey += ce->decal.texGuid + "|";
      lastDecalGuids.push_back(ce->decal.texGuid);
      const TextureRecord* ctr = assets.GetTexByGuid(ce->decal.texGuid);
      if (ctr != nullptr) {
        decalKey += (ctr->loaded ? "1|" : "0|");
      }
      decalTexs.push_back((ctr != nullptr && ctr->loaded) ? &ctr->data : nullptr);
      if (decalTexs.size() >= 16) {
        break;
      }
    }
    if (decalKey != lastDecalKey) {
      lastDecalKey = decalKey;
      lastDecalRects.clear();
      RHITexture atlas = decalTexs.empty() ? 0 : assets.BuildDecalAtlas(decalTexs, lastDecalRects);
      renderer.SetDecalAtlas(atlas);
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
    size_t decalRectIdx = lastDecalGuids.size();
    for (size_t gi = 0; gi < lastDecalGuids.size(); ++gi) {
      if (lastDecalGuids[gi] == e->decal.texGuid) {
        decalRectIdx = gi;
        break;
      }
    }
    if (decalRectIdx * 4 + 3 < lastDecalRects.size()) {
      float rect[4] = { lastDecalRects[decalRectIdx * 4 + 0], lastDecalRects[decalRectIdx * 4 + 1], lastDecalRects[decalRectIdx * 4 + 2], lastDecalRects[decalRectIdx * 4 + 3] };
      renderer.DrawDecalAtlas(dworld, dinv, e->decal.size, fade, rect, e->decal.matMask, e->decal.matRough, e->decal.matMetal);
    } else {
      renderer.DrawDecal(dworld, dinv, e->decal.size, fade, dtex, e->decal.matMask, e->decal.matRough, e->decal.matMetal);
    }
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
  renderer.BakeIrradiance();
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
  Kizuri::LightmapBakeOptions options;
  options.resolution = 128;
  options.format = viewLmFormat == 0 ? Kizuri::TexFormat::Bc1 : (viewLmFormat == 1 ? Kizuri::TexFormat::Bc3 : Kizuri::TexFormat::Bc7);
  options.denoise = viewLmDenoise;
  if (!BakeLightmapForEx(scene, assets, id, options, path, guid, err)) {
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
  ImGui::SameLine();
  ImGui::Checkbox("Simulate", &viewSimulate);
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
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
      if (selection.HasSelection()) {
        FocusEntity(selection.Get());
      } else {
        Announce(LogLevel::Warning, "Selecione uma entidade para focar");
      }
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
  {
    const Kizuri::RenderStats& vst = renderer.GetStats();
    char obuf[128];
    std::snprintf(obuf, sizeof(obuf), "%s  CPU %.1f ms  GPU %.1f ms", fpsShown.c_str(), vst.msTotal, vst.gpuMs);
    vdl->AddText(ImVec2(ipos.x + 8.0f, ipos.y + 8.0f), IM_COL32(235, 235, 245, 255), obuf);
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
  DrawDebugOverlays();
  vdl->ChannelsMerge();
  ImGui::End();
}
static void DrawBoxEdges(ImDrawList* dl, const float m[16], const float* bmin, const float* bmax, const float vf[16], const float pf[16], float viewX, float viewY, float viewW, float viewH, ImU32 col) {
  float c[8][3];
  for (int k = 0; k < 8; ++k) {
    float x = (k & 1) ? bmax[0] : bmin[0];
    float y = (k & 2) ? bmax[1] : bmin[1];
    float z = (k & 4) ? bmax[2] : bmin[2];
    c[k][0] = x * m[0] + y * m[4] + z * m[8] + m[12];
    c[k][1] = x * m[1] + y * m[5] + z * m[9] + m[13];
    c[k][2] = x * m[2] + y * m[6] + z * m[10] + m[14];
  }
  static const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
  for (int k = 0; k < 12; ++k) {
    float ax = 0.0f;
    float ay = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    if (ProjectWorldToScreen(vf, pf, c[edges[k][0]], viewX, viewY, viewW, viewH, ax, ay) &&
        ProjectWorldToScreen(vf, pf, c[edges[k][1]], viewX, viewY, viewW, viewH, bx, by)) {
      dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), col, 1.0f);
    }
  }
}
void EditorApp::DrawDebugOverlays() {
  if (!viewShowBounds && !viewShowLod && !viewShowProbes) {
    return;
  }
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->ChannelsSetCurrent(1);
  float cpx = 0.0f;
  float cpy = 0.0f;
  float cpz = 0.0f;
  camera.GetPosition(cpx, cpy, cpz);
  std::vector<EntityId> all = scene.All();
  size_t drawn = 0;
  for (size_t i = 0; i < all.size() && drawn < 2048; ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    if (viewShowBounds && e->hasMesh && !e->meshGuid.empty()) {
      const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
      if (rec != nullptr) {
        DrawBoxEdges(dl, world, rec->data.aabbMin, rec->data.aabbMax, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, IM_COL32(200, 200, 200, 160));
        ++drawn;
      }
    }
    if (viewShowLod && e->hasLod) {
      float dx = e->transform.position[0] - cpx;
      float dy = e->transform.position[1] - cpy;
      float dz = e->transform.position[2] - cpz;
      float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      ImU32 col = IM_COL32(80, 220, 80, 200);
      if (dist > e->lod.farDist) {
        col = IM_COL32(230, 60, 60, 200);
      } else if (dist > e->lod.nearDist) {
        col = IM_COL32(230, 200, 60, 200);
      }
      float unitMin[3] = { -0.5f, -0.5f, -0.5f };
      float unitMax[3] = { 0.5f, 0.5f, 0.5f };
      DrawBoxEdges(dl, world, unitMin, unitMax, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, col);
      ++drawn;
    }
    if (viewShowProbes && e->hasProbe) {
      float hsize[3] = { e->probe.size[0] * 0.5f, e->probe.size[1] * 0.5f, e->probe.size[2] * 0.5f };
      float nsize[3] = { -hsize[0], -hsize[1], -hsize[2] };
      DrawBoxEdges(dl, world, nsize, hsize, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, IM_COL32(90, 160, 255, 200));
      ++drawn;
    }
    if (e->hasPost) {
      float hsize[3] = { e->post.size[0] * 0.5f, e->post.size[1] * 0.5f, e->post.size[2] * 0.5f };
      float nsize[3] = { -hsize[0], -hsize[1], -hsize[2] };
      DrawBoxEdges(dl, world, nsize, hsize, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, IM_COL32(200, 120, 255, 200));
      ++drawn;
    }
    if (e->hasFog) {
      float hsize[3] = { e->fog.size[0] * 0.5f, e->fog.size[1] * 0.5f, e->fog.size[2] * 0.5f };
      float nsize[3] = { -hsize[0], -hsize[1], -hsize[2] };
      DrawBoxEdges(dl, world, nsize, hsize, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, IM_COL32(120, 200, 220, 200));
      ++drawn;
    }
    if (viewShowPhysics && e->hasRigidbody) {
      ImU32 pcol = IM_COL32(255, 150, 60, 220);
      if (e->rigidbody.colliderType == 0) {
        float ex = e->rigidbody.extents[0] < 0.01f ? 0.5f : e->rigidbody.extents[0];
        float ey = e->rigidbody.extents[1] < 0.01f ? 0.5f : e->rigidbody.extents[1];
        float ez = e->rigidbody.extents[2] < 0.01f ? 0.5f : e->rigidbody.extents[2];
        float nsize[3] = { -ex, -ey, -ez };
        float hsize[3] = { ex, ey, ez };
        DrawBoxEdges(dl, world, nsize, hsize, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, pcol);
        ++drawn;
      } else if (e->rigidbody.colliderType == 1) {
        float radius = e->rigidbody.extents[0] < 0.01f ? 0.5f : e->rigidbody.extents[0];
        float center[3] = { world[12], world[13], world[14] };
        for (int ax = 0; ax < 3; ++ax) {
          float prev[3] = { 0.0f, 0.0f, 0.0f };
          bool hasPrev = false;
          for (int s = 0; s <= 16; ++s) {
            float a = static_cast<float>(s) * 6.2831853f / 16.0f;
            float pt[3] = { center[0], center[1], center[2] };
            pt[(ax + 1) % 3] += std::cos(a) * radius;
            pt[(ax + 2) % 3] += std::sin(a) * radius;
            if (hasPrev) {
              float ax2 = 0.0f;
              float ay2 = 0.0f;
              float bx2 = 0.0f;
              float by2 = 0.0f;
              if (ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], prev, viewX, viewY, viewW, viewH, ax2, ay2) &&
                  ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], pt, viewX, viewY, viewW, viewH, bx2, by2)) {
                dl->AddLine(ImVec2(ax2, ay2), ImVec2(bx2, by2), pcol, 1.0f);
              }
            }
            prev[0] = pt[0];
            prev[1] = pt[1];
            prev[2] = pt[2];
            hasPrev = true;
          }
        }
        ++drawn;
      } else {
        float ex = e->rigidbody.extents[0] < 0.01f ? 0.5f : e->rigidbody.extents[0];
        float ey = e->rigidbody.extents[1] < 0.01f ? 0.5f : e->rigidbody.extents[1];
        float ez = e->rigidbody.extents[2] < 0.01f ? 0.5f : e->rigidbody.extents[2];
        float nsize[3] = { -ex, -ey, -ez };
        float hsize[3] = { ex, ey, ez };
        DrawBoxEdges(dl, world, nsize, hsize, &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, pcol);
        ++drawn;
      }
    }
  }
}
void EditorApp::DrawLightGizmo() {  if (!selection.HasSelection()) {
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
    } else if (e->light.type == static_cast<int>(LightType::Rect)) {
      LightRectPoints(pos, dir, e->light.areaW * 0.5f, e->light.areaH * 0.5f, pts);
      segs = LightRectSegs;
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
void EditorApp::ApplyCustomQuality() {
  const Kizuri::QualitySettings& qs = customQs;
  viewSsao = qs.ssao;
  viewAoMode = qs.aoMode;
  viewSsr = qs.ssr;
  viewSsrTemporal = qs.ssrTemporal;
  viewContact = qs.contact;
  viewSsgi = qs.ssgi;
  viewDof = qs.dof;
  viewMotion = qs.motionBlur;
  viewFxaa = qs.fxaa;
  viewTaa = qs.taa;
  viewTaaSharp = qs.taaSharp;
  viewFog = qs.fog;
  viewBloom = qs.bloom;
  viewScale = qs.renderScale;
  viewShadowFilter = qs.shadowFilter;
  viewVsmQuality = qs.vsmQuality;
  viewShadowRange = qs.shadowRange;
  viewDynRes = qs.dynRes;
  viewOcc = qs.occ;
  viewShadowAuto = qs.shadowAuto;
  viewGodRays = qs.godRays;
  renderer.SetAoMode(viewAoMode);
  renderer.SetShadowFilter(viewShadowFilter);
  renderer.SetVsmQuality(viewVsmQuality);
  renderer.SetShadowRange(viewShadowRange);
  renderer.SetTaa(viewTaa);
  renderer.SetTaaSharpening(viewTaaSharp);
  renderer.SetSsrTemporal(viewSsrTemporal, viewSsrTemporalBlend);
  renderer.SetDynamicResolution(viewDynRes, viewDynTarget, 0.5f, 1.0f);
  renderer.SetOcclusionCulling(viewOcc);
  renderer.SetShadowAutoRes(viewShadowAuto);
  renderer.SetGodRays(viewGodRays, viewGodDensity, viewGodDecay, viewGodIntensity);
}
void EditorApp::SnapshotCustomQuality() {
  customQs.ssao = viewSsao;
  customQs.aoMode = viewAoMode;
  customQs.ssr = viewSsr;
  customQs.ssrTemporal = viewSsrTemporal;
  customQs.contact = viewContact;
  customQs.ssgi = viewSsgi;
  customQs.dof = viewDof;
  customQs.motionBlur = viewMotion;
  customQs.fxaa = viewFxaa;
  customQs.taa = viewTaa;
  customQs.taaSharp = viewTaaSharp;
  customQs.fog = viewFog;
  customQs.bloom = viewBloom;
  customQs.renderScale = viewScale;
  customQs.shadowFilter = viewShadowFilter;
  customQs.vsmQuality = viewVsmQuality;
  customQs.shadowRange = viewShadowRange;
  customQs.dynRes = viewDynRes;
  customQs.occ = viewOcc;
  customQs.shadowAuto = viewShadowAuto;
  customQs.godRays = viewGodRays;
}
void EditorApp::DrawSettings() {
  ImGui::Begin("Settings", &showSettings);
  if (ImGui::BeginTabBar("SettingsTabs")) {
    if (ImGui::BeginTabItem("Post")) {
      ImGui::SliderFloat("Exposure", &viewExposure, 0.25f, 4.0f);
      int tm = viewToneMap < 0 ? 0 : (viewToneMap > 3 ? 3 : viewToneMap);
      if (ImGui::Combo("Tone Mapper", &tm, "ACES\0Reinhard\0AgX\0None\0")) {
        viewToneMap = tm;
        renderer.SetToneMapping(tm);
      }
      ImGui::Checkbox("Auto Exposure", &viewAutoExp);
      if (viewAutoExp && ImGui::Checkbox("Robust Histogram", &viewRobustExp)) {
        renderer.SetRobustExposure(viewRobustExp);
      }
      ImGui::SliderFloat("Contrast", &viewContrast, 0.0f, 2.0f);
      ImGui::SliderFloat("Saturation", &viewSaturation, 0.0f, 3.0f);
      ImGui::SliderFloat("Temperature", &viewTemp, -1.0f, 1.0f);
      ImGui::SliderFloat("Tint", &viewTint, -1.0f, 1.0f);
      ImGui::SliderFloat("LUT Amount", &viewLutAmount, 0.0f, 1.0f);
      int lp = viewLutPreset < 0 ? 0 : (viewLutPreset > 5 ? 5 : viewLutPreset);
      if (ImGui::Combo("LUT Preset", &lp, "None\0Warm\0Cool\0Noir\0Vintage\0Custom\0")) {
        viewLutPreset = lp;
        if (lp == 0) {
          renderer.SetLutAmount(0.0f);
          viewLutAmount = 0.0f;
        } else if (lp < 5) {
          if (renderer.SetLutPreset(lp)) {
            viewLutAmount = 1.0f;
            renderer.SetLutAmount(1.0f);
          }
        }
      }
      if (ImGui::Button("Load .cube LUT")) {
        std::string picked;
        if (ShowOpenCubeDialog(window.NativeHandle(), picked)) {
          std::vector<unsigned char> lutPx;
          int lutSize = 0;
          if (!Kizuri::ParseCubeLut(picked, lutPx, lutSize)) {
            Announce(LogLevel::Error, std::string("LUT parse failed: ") + picked);
          } else if (!renderer.UploadLut(lutPx.data(), lutSize)) {
            Announce(LogLevel::Error, "LUT upload failed");
          } else {
            viewLutPreset = 5;
            viewLutAmount = 1.0f;
            renderer.SetLutAmount(1.0f);
            log.Add(LogLevel::Success, std::string("LUT loaded: ") + picked);
          }
        }
      }
      ImGui::SliderFloat("Bloom", &viewBloom, 0.0f, 2.0f);
      ImGui::Separator();
      ImGui::Checkbox("SSAO", &viewSsao);
      int am = viewAoMode < 0 ? 0 : (viewAoMode > 3 ? 3 : viewAoMode);
      if (ImGui::Combo("AO Mode", &am, "Off\0SSAO\0HBAO\0GTAO\0")) {
        viewAoMode = am;
        renderer.SetAoMode(am);
      }
      ImGui::SliderFloat("SSAO Intensity", &viewSsaoIntensity, 0.0f, 2.0f);
      ImGui::SliderFloat("SSAO Radius", &viewSsaoRadius, 0.1f, 2.0f);
      ImGui::Separator();
      ImGui::Checkbox("SSR", &viewSsr);
      if (ImGui::Checkbox("SSR Temporal", &viewSsrTemporal)) {
        renderer.SetSsrTemporal(viewSsrTemporal, viewSsrTemporalBlend);
      }
      if (viewSsrTemporal && ImGui::SliderFloat("SSR Blend", &viewSsrTemporalBlend, 0.0f, 0.95f)) {
        renderer.SetSsrTemporal(true, viewSsrTemporalBlend);
      }
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
      if (ImGui::SliderFloat("Heat Haze", &viewHeat, 0.0f, 1.0f)) {
        renderer.SetHeatHaze(viewHeat);
      }
      if (ImGui::SliderFloat("Lens Dirt", &viewLensDirt, 0.0f, 2.0f)) {
        renderer.SetLensDirt(viewLensDirt);
      }
      ImGui::Separator();
      ImGui::Checkbox("Fog", &viewFog);
      ImGui::SliderFloat("Fog Density", &viewFogDensity, 0.0f, 0.05f);
      ImGui::ColorEdit3("Fog Color", viewFogColor);
      ImGui::SliderFloat("Height Falloff", &viewFogHeight, 0.0f, 1.0f);
      ImGui::SliderFloat("Height Base", &viewFogBase, -10.0f, 50.0f);
      ImGui::Separator();
      ImGui::Checkbox("God Rays", &viewGodRays);
      ImGui::SliderFloat("Shaft Density", &viewGodDensity, 0.1f, 3.0f);
      ImGui::SliderFloat("Shaft Decay", &viewGodDecay, 0.8f, 0.999f);
      ImGui::SliderFloat("Shaft Intensity", &viewGodIntensity, 0.0f, 2.0f);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Render")) {
      int q = viewQuality < 0 ? 2 : (viewQuality > 4 ? 4 : viewQuality);
      if (ImGui::Combo("Quality", &q, "Low\0Medium\0High\0Ultra\0Custom\0")) {
        viewQuality = q;
        if (q == 4) {
          ApplyCustomQuality();
        } else {
        Kizuri::QualitySettings qs = Kizuri::ApplyQualityPreset(static_cast<Kizuri::QualityLevel>(q));
        viewSsao = qs.ssao;
        viewAoMode = qs.aoMode;
        viewSsr = qs.ssr;
        viewSsrTemporal = qs.ssrTemporal;
        viewContact = qs.contact;
        viewSsgi = qs.ssgi;
        viewDof = qs.dof;
        viewMotion = qs.motionBlur;
        viewFxaa = qs.fxaa;
        viewTaa = qs.taa;
        viewTaaSharp = qs.taaSharp;
        viewFog = qs.fog;
        viewBloom = qs.bloom;
        viewScale = qs.renderScale;
        viewShadowFilter = qs.shadowFilter;
        viewVsmQuality = qs.vsmQuality;
        viewShadowRange = qs.shadowRange;
        viewDynRes = qs.dynRes;
        viewOcc = qs.occ;
        viewShadowAuto = qs.shadowAuto;
        viewGodRays = qs.godRays;
        renderer.SetAoMode(viewAoMode);
        renderer.SetShadowFilter(viewShadowFilter);
        renderer.SetVsmQuality(viewVsmQuality);
        renderer.SetShadowRange(viewShadowRange);
  renderer.SetTaa(viewTaa);
  renderer.SetTaaSharpening(viewTaaSharp);
  renderer.SetDdgi(viewDdgi, viewDdgiWeight);
  renderer.SetProbeAutoRefresh(viewProbeRefresh);
        renderer.SetSsrTemporal(viewSsrTemporal, viewSsrTemporalBlend);
        renderer.SetDynamicResolution(viewDynRes, viewDynTarget, 0.5f, 1.0f);
        renderer.SetOcclusionCulling(viewOcc);
        renderer.SetShadowAutoRes(viewShadowAuto);
        renderer.SetGodRays(viewGodRays, viewGodDensity, viewGodDecay, viewGodIntensity);
        }
      }
      if (ImGui::Button("Save Custom")) {
        SnapshotCustomQuality();
        SaveEditorSettings();
        if (viewQuality == 4) {
          ApplyCustomQuality();
        }
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
      if (ImGui::Checkbox("TAA", &viewTaa)) {
        renderer.SetTaa(viewTaa);
      }
      if (viewTaa && ImGui::SliderFloat("TAA Sharpness", &viewTaaSharp, 0.0f, 1.0f)) {
        renderer.SetTaaSharpening(viewTaaSharp);
      }
      ImGui::SliderFloat("Env Intensity", &viewEnv, 0.0f, 4.0f);
      if (ImGui::Checkbox("DDGI Volume", &viewDdgi)) {
  renderer.SetDdgi(viewDdgi, viewDdgiWeight);
  renderer.SetAmbientProbe(viewAmbient[0], viewAmbient[1], viewAmbient[2]);
      }
      if (viewDdgi && ImGui::SliderFloat("DDGI Weight", &viewDdgiWeight, 0.0f, 2.0f)) {
        renderer.SetDdgi(true, viewDdgiWeight);
      }
      if (ImGui::ColorEdit3("Ambient Probe", viewAmbient)) {
  renderer.SetAmbientProbe(viewAmbient[0], viewAmbient[1], viewAmbient[2]);
  renderer.SetSssRadius(viewSssRadius);
      }
      if (ImGui::SliderFloat("SSS Radius", &viewSssRadius, 0.0f, 8.0f)) {
        renderer.SetSssRadius(viewSssRadius);
      }
      if (ImGui::Button("Bake Environment")) {
        BakeEnvironment();
      }
      if (ImGui::Button("Bake Probes")) {
        BakeProbes();
      }
      if (ImGui::Checkbox("Probe Auto Refresh", &viewProbeRefresh)) {
        renderer.SetProbeAutoRefresh(viewProbeRefresh);
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Debug")) {
      ImGui::Checkbox("Debug CSM", &debugCascades);
      if (ImGui::Checkbox("Occlusion Culling", &viewOcc)) {
        renderer.SetOcclusionCulling(viewOcc);
      }
      if (ImGui::Checkbox("GPU Driven", &viewGpuDriven)) {
        renderer.SetGpuDriven(viewGpuDriven);
      }
      if (ImGui::Checkbox("Temporal Upsampling", &viewTempUpsample)) {
        renderer.SetTemporalUpsampling(viewTempUpsample);
      }
      if (ImGui::Checkbox("Shadow Multithread", &viewMtShadows)) {
        renderer.SetShadowMultithread(viewMtShadows);
      }
      if (ImGui::SliderFloat("Target FPS", &viewTargetFps, 0.0f, 240.0f)) {
        renderer.SetTargetFrameRate(viewTargetFps);
      }
      ImGui::Checkbox("Static Batching", &viewStaticBatch);
      ImGui::Checkbox("Meshlet Culling", &viewMeshletCull);
      ImGui::Combo("Proc Mode", &viewProcMode, "Checker\0Noise\0Gradient\0Flat Normal\0");
      ImGui::SliderInt("Proc Size", &viewProcSize, 16, 512);
      if (ImGui::Button("Create Procedural")) {
        viewProcGuid = assets.CreateProceduralTexture(viewProcMode, viewProcSize, viewProcSize);
      }
      if (!viewProcGuid.empty()) {
        ImGui::TextWrapped("%s", viewProcGuid.c_str());
      }
      if (ImGui::Checkbox("Shadow Auto Res", &viewShadowAuto)) {
        renderer.SetShadowAutoRes(viewShadowAuto);
      }
      int sf = viewShadowFilter < 0 ? 0 : (viewShadowFilter > 3 ? 3 : viewShadowFilter);
      if (ImGui::Combo("Shadow Filter", &sf, "PCSS\0PCF 3x3\0VSM\0EVSM\0")) {
        viewShadowFilter = sf;
        renderer.SetShadowFilter(sf);
      }
      if (sf >= 2) {
        if (ImGui::Combo("VSM Quality", &viewVsmQuality, "Full\0Half\0Quarter\0")) {
          renderer.SetVsmQuality(viewVsmQuality);
        }
        if (ImGui::SliderFloat("VSM Bleed", &viewVsmBleed, 0.0f, 0.9f)) {
          renderer.SetVsmBleed(viewVsmBleed);
        }
        if (sf >= 3 && ImGui::SliderFloat("EVSM Exponent", &viewEvsmExp, 0.5f, 5.0f)) {
          renderer.SetEvsmExponent(viewEvsmExp);
        }
      }
      if (ImGui::SliderFloat("Shadow Range (0=off)", &viewShadowRange, 0.0f, 200.0f)) {
        renderer.SetShadowRange(viewShadowRange);
      }
      if (ImGui::Combo("Attenuation", &viewAttenMode, "Smooth\0Physical\0Linear\0")) {
        renderer.SetAttenuationMode(viewAttenMode);
      }
      if (ImGui::SliderFloat("Light Scale", &viewPhysScale, 0.01f, 100.0f)) {
        renderer.SetPhysicalScale(viewPhysScale);
      }
      if (ImGui::SliderInt("Light Layers", &viewLightLayers, 0, 255)) {
        renderer.SetCameraLightMask(static_cast<uint32_t>(viewLightLayers));
      }
      if (ImGui::Checkbox("Spot Cookie", &viewSpotCookie)) {
  renderer.SetSpotCookieOn(viewSpotCookie);
  renderer.SetClustered(viewClustered);
  renderer.SetGpuCulling(viewGpuCull);
  renderer.SetForwardPlus(viewForwardPlus);
      }
      if (ImGui::Checkbox("Clustered", &viewClustered)) {
        renderer.SetClustered(viewClustered);
      }
      if (ImGui::Checkbox("GPU Culling", &viewGpuCull)) {
        renderer.SetGpuCulling(viewGpuCull);
      }
      if (ImGui::Checkbox("Forward+", &viewForwardPlus)) {
        renderer.SetForwardPlus(viewForwardPlus);
      }
      if (ImGui::Checkbox("Tessellation", &viewTess)) {
        renderer.SetTessellation(viewTess, viewTessFactor, viewTessDisp);
      }
      if (viewTess) {
        if (ImGui::SliderFloat("Tess Factor", &viewTessFactor, 1.0f, 16.0f)) {
          renderer.SetTessellation(true, viewTessFactor, viewTessDisp);
        }
        if (ImGui::SliderFloat("Tess Disp", &viewTessDisp, 0.0f, 2.0f)) {
          renderer.SetTessellation(true, viewTessFactor, viewTessDisp);
        }
      }
      int dv = viewDebug < 0 ? 0 : (viewDebug > 18 ? 18 : viewDebug);
      if (ImGui::Combo("View", &dv, "Final\0Wireframe\0Unlit\0Albedo+AO\0Normal\0Roughness\0Metallic\0AO\0Emissive\0Depth\0Motion\0ShadowMap\0Probe\0Lightmap\0MatID\0Overdraw\0UV\0Custom\0HZB\0")) {
        viewDebug = dv;
        renderer.SetDebugView(dv);
      }
      ImGui::Checkbox("Show Bounds", &viewShowBounds);
      ImGui::Checkbox("Show LOD Tiers", &viewShowLod);
      ImGui::Checkbox("Show Probes", &viewShowProbes);
      {
        std::vector<std::string> tg = assets.AllTexGuids();
        std::string items = "None";
        int cur = 0;
        for (size_t ti = 0; ti < tg.size(); ++ti) {
          const TextureRecord* tr = assets.GetTexByGuid(tg[ti]);
          std::string fn = (tr != nullptr) ? std::filesystem::path(tr->sourcePath).filename().string() : tg[ti];
          if (fn.empty()) {
            fn = tg[ti];
          }
          items += std::string("\0") + fn;
          if (tg[ti] == viewCookieGuid) {
            cur = static_cast<int>(ti) + 1;
          }
        }
        if (ImGui::Combo("Cookie Tex", &cur, items.c_str())) {
          viewCookieGuid.clear();
          Kizuri::RHITexture ck = 0;
          if (cur >= 1 && static_cast<size_t>(cur - 1) < tg.size()) {
            viewCookieGuid = tg[static_cast<size_t>(cur - 1)];
            const TextureRecord* tr = assets.GetTexByGuid(viewCookieGuid);
            if (tr != nullptr) {
              ck = tr->gpu;
            }
          }
          renderer.SetSpotCookie(ck);
        }
      }
      if (ImGui::SliderFloat("Normal Bias", &viewNormalBias, 0.0f, 4.0f)) {
        renderer.SetNormalBias(viewNormalBias);
      }
      if (ImGui::Checkbox("Auto Bias", &viewAutoBias)) {
        renderer.SetAutoBias(viewAutoBias);
      }
      const Kizuri::RenderStats& rst = renderer.GetStats();
      ImGui::Text("%s", fpsShown.c_str());
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
      ImGui::Separator();
      ImGui::Text("Docking");
      {
        const char* names[5] = { "Viewport", "Hierarchy", "Inspector", "Console", "Asset Browser" };
        for (int i = 0; i < 5; ++i) {
          ImGuiWindow* w = ImGui::FindWindowByName(names[i]);
          if (w == nullptr) {
            ImGui::Text("%s: inexistente", names[i]);
            continue;
          }
          if (w->DockNode == nullptr) {
            ImGui::Text("%s: FLUTUANTE pos %.0f,%.0f tam %.0fx%.0f", names[i], w->Pos.x, w->Pos.y, w->Size.x, w->Size.y);
            continue;
          }
          ImGui::Text("%s: dockado em %.0f,%.0f tam %.0fx%.0f", names[i], w->DockNode->Pos.x, w->DockNode->Pos.y, w->DockNode->Size.x, w->DockNode->Size.y);
        }
        ImGui::Text("nos: esq %u dir %u baixo %u centro %u", (unsigned int)dockLeftId, (unsigned int)dockRightId, (unsigned int)dockBottomId, (unsigned int)dockCenterId);
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}
namespace {
bool NameContainsFilter(const std::string& name, const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  std::string lower = name;
  std::string needle = filter;
  for (size_t i = 0; i < lower.size(); ++i) {
    lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
  }
  for (size_t i = 0; i < needle.size(); ++i) {
    needle[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
  }
  return lower.find(needle) != std::string::npos;
}
}
void EditorApp::DrawHierarchyTree(EntityId parent, int depth, const std::map<uint32_t, std::vector<EntityId>>& byParent, const std::string& filter) {
  const uint32_t key = parent.IsValid() ? parent.index : 0xFFFFFFFFu;
  std::map<uint32_t, std::vector<EntityId>>::const_iterator it = byParent.find(key);
  if (it == byParent.end()) {
    return;
  }
  for (size_t i = 0; i < it->second.size(); ++i) {
    const EntityId id = it->second[i];
    Entity* e = scene.Get(id);
    if (e == nullptr) {
      continue;
    }
    bool visible = filter.empty() || NameContainsFilter(e->name, filter);
    if (!visible) {
      std::map<uint32_t, std::vector<EntityId>>::const_iterator cit = byParent.find(id.index);
      if (cit != byParent.end()) {
        for (size_t c = 0; c < cit->second.size() && !visible; ++c) {
          const Entity* ce = scene.Get(cit->second[c]);
          if (ce != nullptr && NameContainsFilter(ce->name, filter)) {
            visible = true;
          }
        }
      }
      if (!visible) {
        continue;
      }
    }
    ImGui::PushID(static_cast<int>(id.index * 1315423911u + id.generation));
    const std::map<uint32_t, std::vector<EntityId>>::const_iterator hit = byParent.find(id.index);
    const bool hasChildren = hit != byParent.end() && !hit->second.empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    bool open = true;
    if (renameActive && renameTarget == id) {
      open = ImGui::TreeNodeEx("##node", flags, "%s", "");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1.0f);
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##rename", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string oldName = e->name;
        const std::string newName = renameBuf;
        if (oldName != newName) {
          std::unique_ptr<Command> cmd(new RenameCmd(id, oldName, newName));
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
      open = ImGui::TreeNodeEx("##node", flags, "%s", e->name.c_str());
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        ImGuiIO& hio = ImGui::GetIO();
        if (hio.KeyShift) {
          selection.Add(id);
        } else if (hio.KeyCtrl) {
          selection.Toggle(id);
        } else {
          selection.Select(id);
        }
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        renameActive = true;
        renameTarget = id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
    }
    if (ImGui::BeginPopupContextItem("EntityContext")) {
      if (ImGui::MenuItem("Create Entity")) {
        CreateEntityAt(0.0f, 0.0f, 0.0f);
      }
      if (ImGui::MenuItem("Duplicate")) {
        DuplicateViaCommand(id);
      }
      if (ImGui::MenuItem("Delete")) {
        DeleteViaCommand(id);
      }
      if (ImGui::MenuItem("Rename")) {
        renameActive = true;
        renameTarget = id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Focus")) {
        FocusEntity(id);
      }
      ImGui::EndPopup();
    }
    if (open) {
      DrawHierarchyTree(id, depth + 1, byParent, filter);
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
}
void EditorApp::DrawHierarchy() {
  ImGui::Begin("Hierarchy", &showHierarchy);
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##hierSearch", "Buscar...", hierSearchBuf, sizeof(hierSearchBuf));
  const std::string filter = hierSearchBuf;
  if (ImGui::Button("Criar Entidade")) {
    CreateEntityAt(0.0f, 0.0f, 0.0f);
  }
  ImGui::Separator();
  std::map<uint32_t, std::vector<EntityId>> byParent;
  const std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    const uint32_t key = e->parent.IsValid() ? e->parent.index : 0xFFFFFFFFu;
    byParent[key].push_back(all[i]);
  }
  DrawHierarchyTree(EntityId::Invalid(), 0, byParent, filter);
  if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
    if (ImGui::MenuItem("Criar Entidade")) {
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
  bool stat = e->isStatic;
  PropertyLabel("Static");
  if (ImGui::Checkbox("##Static", &stat)) {
    edits.PushStatic(e->id, stat);
  }
  PropertySection("Transform");
  Transform t = e->transform;
  bool changed = false;
  PropertyLabel("Position");
  if (ImGui::DragFloat3("##Position", t.position, 0.05f)) {
    changed = true;
  }
  PropertyLabel("Rotation");
  if (ImGui::DragFloat3("##Rotation", t.rotation, 0.5f)) {
    changed = true;
  }
  PropertyLabel("Scale");
  if (ImGui::DragFloat3("##Scale", t.scale, 0.02f)) {
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
    ImGui::PushID("Mesh");
    PropertySection("Mesh Renderer");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##mesh")) {
      std::unique_ptr<Command> cmd(new RemoveMeshCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    DrawMeshSection(e);
    ImGui::PopID();
  }
  if (e->hasLight) {
    ImGui::PushID("Light");
    PropertySection("Light");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##light")) {
      std::unique_ptr<Command> cmd(new RemoveLightCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    ImGui::SameLine();
    bool soloed = soloLightId.IsValid() && soloLightId == e->id;
    if (ImGui::SmallButton(soloed ? "Unsolo##light" : "Solo##light")) {
      if (soloed) {
        soloLightId = EntityId::Invalid();
      } else {
        soloLightId = e->id;
      }
    }
    LightData light = e->light;
    int lightMode = light.type;
    if (lightMode < 0 || lightMode > 3) {
      lightMode = 0;
    }
    bool lightChanged = false;
    PropertyLabel("Type");
    if (ImGui::Combo("##Type", &lightMode, "Point\0Spot\0Directional\0Rect\0")) {
      light.type = lightMode;
      lightChanged = true;
    }
    PropertyLabel("Link Mask");
    if (ImGui::SliderInt("##Link Mask", &light.linkMask, 0, 255)) {
      lightChanged = true;
    }
    PropertyLabel("Layer");
    if (ImGui::SliderInt("##Layer", &light.layer, 0, 31)) {
      lightChanged = true;
    }
    PropertyLabel("Color");
    if (ImGui::ColorEdit3("##Color", light.color)) {
      lightChanged = true;
    }
    PropertyLabel("Intensity");
    if (ImGui::DragFloat("##Intensity", &light.intensity, 0.05f, 0.0f, 20.0f)) {
      lightChanged = true;
    }
    if (light.type == static_cast<int>(LightType::Point) || light.type == static_cast<int>(LightType::Spot) || light.type == static_cast<int>(LightType::Rect)) {
      PropertyLabel("Radius");
      if (ImGui::DragFloat("##Radius", &light.radius, 0.1f, 0.5f, 100.0f)) {
        lightChanged = true;
      }
    }
    if (light.type == static_cast<int>(LightType::Rect)) {
      PropertyLabel("Area W");
      if (ImGui::DragFloat("##Area W", &light.areaW, 0.05f, 0.1f, 50.0f)) {
        lightChanged = true;
      }
      PropertyLabel("Area H");
      if (ImGui::DragFloat("##Area H", &light.areaH, 0.05f, 0.1f, 50.0f)) {
        lightChanged = true;
      }
    }
    if (light.type == static_cast<int>(LightType::Spot)) {
      PropertyLabel("Angle");
      if (ImGui::DragFloat("##Angle", &light.spotAngle, 0.5f, 1.0f, 170.0f)) {
        lightChanged = true;
      }
      PropertyLabel("Falloff");
      if (ImGui::DragFloat("##Falloff", &light.falloff, 0.01f, 0.01f, 0.5f)) {
        lightChanged = true;
      }
      {
        std::vector<std::string> ctg = assets.AllTexGuids();
        std::string citems = "Default";
        int ccur = 0;
        for (size_t cti = 0; cti < ctg.size(); ++cti) {
          const TextureRecord* ctr = assets.GetTexByGuid(ctg[cti]);
          std::string cfn = (ctr != nullptr) ? std::filesystem::path(ctr->sourcePath).filename().string() : ctg[cti];
          if (cfn.empty()) {
            cfn = ctg[cti];
          }
          citems += std::string("\0") + cfn;
          if (ctg[cti] == light.cookieGuid) {
            ccur = static_cast<int>(cti) + 1;
          }
        }
        PropertyLabel("Cookie");
        if (ImGui::Combo("##Cookie", &ccur, citems.c_str())) {
          light.cookieGuid.clear();
          if (ccur >= 1 && static_cast<size_t>(ccur - 1) < ctg.size()) {
            light.cookieGuid = ctg[static_cast<size_t>(ccur - 1)];
          }
          lightChanged = true;
        }
      }
    }
    bool showShadow = light.castShadow;
    PropertyLabel("Cast Shadow");
    if (ImGui::Checkbox("##Cast Shadow", &showShadow)) {
      light.castShadow = showShadow;
      lightChanged = true;
    }
    if (light.castShadow) {
      PropertyLabel("Light Size");
      if (ImGui::DragFloat("##Light Size", &light.lightSize, 0.01f, 0.0f, 2.0f)) {
        lightChanged = true;
      }
      int resMode = 1;
      if (light.shadowSize == 512) {
        resMode = 0;
      } else if (light.shadowSize == 2048) {
        resMode = 2;
      }
      PropertyLabel("Shadow Size");
      if (ImGui::Combo("##Shadow Size", &resMode, "512\0""1024\0""2048\0")) {
        light.shadowSize = resMode == 0 ? 512 : (resMode == 2 ? 2048 : 1024);
        lightChanged = true;
      }
      PropertyLabel("Softness");
      if (ImGui::DragFloat("##Softness", &light.softness, 0.01f, 0.0f, 2.0f)) {
        lightChanged = true;
      }
      if (light.type == static_cast<int>(LightType::Directional)) {
        PropertyLabel("Cascades");
        if (ImGui::SliderInt("##Cascades", &light.cascades, 2, 4)) {
          lightChanged = true;
        }
        PropertyLabel("Lambda");
        if (ImGui::DragFloat("##Lambda", &light.lambda, 0.01f, 0.0f, 1.0f)) {
          lightChanged = true;
        }
      }
    }
    if (lightChanged) {
      edits.PushLight(e->id, light);
    }
    ImGui::PopID();
  }
  if (e->hasMaterial) {
    ImGui::PushID("Material");
    PropertySection("Material");
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
    ImGui::PopID();
  }
  if (e->hasDecal) {
    ImGui::PushID("Decal");
    PropertySection("Decal");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##decal")) {
      std::unique_ptr<Command> cmd(new RemoveDecalCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    DecalData dec = e->decal;
    bool decChanged = false;
    PropertyLabel("Decal Size");
    if (ImGui::DragFloat3("##Decal Size", dec.size, 0.05f, 0.05f, 50.0f)) {
      decChanged = true;
    }
    PropertyLabel("Life");
    if (ImGui::DragFloat("##Life", &dec.life, 0.1f, 0.0f, 300.0f)) {
      decChanged = true;
    }
    int dmask = dec.matMask < 0 ? 0 : (dec.matMask > 15 ? 15 : dec.matMask);
    PropertyLabel("Channels");
    if (ImGui::Combo("##Channels", &dmask, "Off\0Albedo\0Albedo+Normal/Rough\0Full\0Full+Emissive\0")) {
      dec.matMask = dmask == 0 ? 0 : (dmask == 1 ? 1 : (dmask == 2 ? 3 : (dmask == 3 ? 7 : 15)));
      decChanged = true;
    }
    if (dec.matMask >= 2) {
      PropertyLabel("Decal Rough");
      if (ImGui::SliderFloat("##Decal Rough", &dec.matRough, 0.0f, 1.0f)) {
        decChanged = true;
      }
      PropertyLabel("Decal Metal");
      if (ImGui::SliderFloat("##Decal Metal", &dec.matMetal, 0.0f, 1.0f)) {
        decChanged = true;
      }
    }
    if (decChanged) {
      edits.PushDecal(e->id, dec);
    }
    DrawDecalSection(e);
    ImGui::PopID();
  }
  if (e->hasLod) {
    ImGui::PushID("Lod");
    PropertySection("LOD");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##lod")) {
      std::unique_ptr<Command> cmd(new RemoveLodCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    LodData lod = e->lod;
    bool lodChanged = false;
    PropertyLabel("Near Dist");
    if (ImGui::DragFloat("##Near Dist", &lod.nearDist, 0.5f, 1.0f, 500.0f)) {
      lodChanged = true;
    }
    PropertyLabel("Far Dist");
    if (ImGui::DragFloat("##Far Dist", &lod.farDist, 0.5f, 2.0f, 1000.0f)) {
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
      const MeshRecord* autoBase = assets.GetByGuid(e->meshGuid);
      if (autoBase != nullptr) {
        int autoFound = 0;
        for (int t = 1; t <= 2; ++t) {
          std::string sp = Kizuri::LodSiblingPath(autoBase->meshPath, t);
          if (!sp.empty() && assets.GetByMeshPath(sp) != nullptr) {
            ++autoFound;
          }
        }
        if (autoFound > 0) {
          ImGui::Text("Auto LOD tiers: %d/2", autoFound);
        } else {
          ImGui::TextDisabled("Auto LOD tiers: none");
        }
      }
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
    ImGui::PopID();
  }
  if (e->hasLightmap) {
    ImGui::PushID("Lightmap");
    PropertySection("Lightmap");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##lightmap")) {
      std::unique_ptr<Command> cmd(new RemoveLightmapCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    LightmapData lm = e->lightmap;
    bool lmChanged = false;
    PropertyLabel("Lightmap Intensity");
    if (ImGui::DragFloat("##Lightmap Intensity", &lm.intensity, 0.05f, 0.0f, 4.0f)) {
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
    PropertyLabel("LM Format");
    if (ImGui::Combo("##LM Format", &viewLmFormat, "BC1\0BC3\0BC7\0")) {
    }
    PropertyLabel("LM Denoise");
    if (ImGui::SliderFloat("##LM Denoise", &viewLmDenoise, 0.0f, 3.0f)) {
    }
    ImGui::PopID();
  }
  if (e->hasProbe) {
    ImGui::PushID("Probe");
    PropertySection("Reflection Probe");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##probe")) {
      std::unique_ptr<Command> cmd(new RemoveProbeCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    ProbeData pr = e->probe;
    bool prChanged = false;
    PropertyLabel("Probe Size");
    if (ImGui::DragFloat3("##Probe Size", pr.size, 0.1f, 0.5f, 100.0f)) {
      prChanged = true;
    }
    PropertyLabel("Probe Intensity");
    if (ImGui::DragFloat("##Probe Intensity", &pr.intensity, 0.05f, 0.0f, 4.0f)) {
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
    ImGui::PopID();
  }
  if (e->hasPost) {
    ImGui::PushID("Post");
    PropertySection("Post Volume");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##post")) {
      std::unique_ptr<Command> cmd(new RemovePostCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    PostData pv = e->post;
    bool pvChanged = false;
    PropertyLabel("Volume Size");
    if (ImGui::DragFloat3("##Volume Size", pv.size, 0.1f, 0.5f, 200.0f)) {
      pvChanged = true;
    }
    PropertyLabel("Exposure");
    if (ImGui::DragFloat("##Exposure", &pv.exposure, 0.01f, 0.05f, 8.0f)) {
      pvChanged = true;
    }
    int ptm = pv.toneMap < 0 ? 0 : (pv.toneMap > 3 ? 3 : pv.toneMap);
    PropertyLabel("Tone Mapper");
    if (ImGui::Combo("##Tone Mapper", &ptm, "ACES\0Reinhard\0AgX\0None\0")) {
      pv.toneMap = ptm;
      pvChanged = true;
    }
    PropertyLabel("Contrast");
    if (ImGui::DragFloat("##Contrast", &pv.contrast, 0.01f, 0.2f, 2.0f)) {
      pvChanged = true;
    }
    PropertyLabel("Saturation");
    if (ImGui::DragFloat("##Saturation", &pv.saturation, 0.01f, 0.0f, 3.0f)) {
      pvChanged = true;
    }
    PropertyLabel("Bloom");
    if (ImGui::DragFloat("##Bloom", &pv.bloom, 0.01f, 0.0f, 2.0f)) {
      pvChanged = true;
    }
    if (pvChanged) {
      edits.PushPost(e->id, pv);
    }
    ImGui::PopID();
  }
  if (e->hasFog) {
    ImGui::PushID("Fog");
    PropertySection("Fog Volume");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##fog")) {
      std::unique_ptr<Command> cmd(new RemoveFogCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    FogData fg = e->fog;
    bool fgChanged = false;
    PropertyLabel("Volume Size");
    if (ImGui::DragFloat3("##Volume Size", fg.size, 0.1f, 0.5f, 200.0f)) {
      fgChanged = true;
    }
    PropertyLabel("Density");
    if (ImGui::DragFloat("##Density", &fg.density, 0.001f, 0.0f, 1.0f)) {
      fgChanged = true;
    }
    PropertyLabel("Fog Color");
    if (ImGui::ColorEdit3("##Fog Color", fg.color)) {
      fgChanged = true;
    }
    if (fgChanged) {
      edits.PushFog(e->id, fg);
    }
    ImGui::PopID();
  }
  if (e->hasRigidbody) {
    ImGui::PushID("Rigidbody");
    PropertySection("Rigidbody");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##rigidbody")) {
      std::unique_ptr<Command> cmd(new RemoveRigidbodyCmd(e->id));
      undo.Execute(std::move(cmd), scene);
    }
    RigidbodyData rb = e->rigidbody;
    bool rbChanged = false;
    int motion = rb.motionType < 0 ? 0 : (rb.motionType > 2 ? 2 : rb.motionType);
    PropertyLabel("Motion");
    if (ImGui::Combo("##Motion", &motion, "Static\0Kinematic\0Dynamic\0")) {
      rb.motionType = motion;
      rbChanged = true;
    }
    PropertyLabel("Mass");
    if (rb.motionType == 2 && ImGui::DragFloat("##Mass", &rb.mass, 0.1f, 0.01f, 1000.0f)) {
      rbChanged = true;
    }
    int collider = rb.colliderType < 0 ? 0 : (rb.colliderType > 3 ? 3 : rb.colliderType);
    PropertyLabel("Collider");
    if (ImGui::Combo("##Collider", &collider, "Box\0Sphere\0Capsule\0Mesh\0")) {
      rb.colliderType = collider;
      rbChanged = true;
    }
    PropertyLabel("Half Extents");
    if (rb.colliderType == 0 && ImGui::DragFloat3("##Half Extents", rb.extents, 0.05f, 0.01f, 100.0f)) {
      rbChanged = true;
    }
    PropertyLabel("Radius");
    if (rb.colliderType == 1 && ImGui::DragFloat("##Radius", &rb.extents[0], 0.05f, 0.01f, 100.0f)) {
      rbChanged = true;
    }
    if (rb.colliderType == 2) {
      PropertyLabel("Radius");
      if (ImGui::DragFloat("##Radius", &rb.extents[0], 0.05f, 0.01f, 100.0f)) {
        rbChanged = true;
      }
      PropertyLabel("Half Height");
      if (ImGui::DragFloat("##Half Height", &rb.extents[1], 0.05f, 0.01f, 100.0f)) {
        rbChanged = true;
      }
    }
    if (rb.colliderType == 3) {
      ImGui::TextDisabled("Uses entity mesh triangles (static only)");
    }
    PropertyLabel("Friction");
    if (ImGui::DragFloat("##Friction", &rb.friction, 0.01f, 0.0f, 2.0f)) {
      rbChanged = true;
    }
    PropertyLabel("Restitution");
    if (ImGui::DragFloat("##Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f)) {
      rbChanged = true;
    }
    if (rbChanged) {
      edits.PushRigidbody(e->id, rb);
    }
    ImGui::PopID();
  }
  DrawAudioSourceSection(e);
  if (!e->hasMesh || !e->hasLight || !e->hasMaterial || !e->hasDecal || !e->hasLod || !e->hasLightmap || !e->hasProbe || !e->hasPost || !e->hasFog || !e->hasRigidbody || !e->hasAudioSource) {
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
      if (!e->hasPost && ImGui::MenuItem("Post Volume")) {
        std::unique_ptr<Command> cmd(new AddPostCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasFog && ImGui::MenuItem("Fog Volume")) {
        std::unique_ptr<Command> cmd(new AddFogCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasRigidbody && ImGui::MenuItem("Rigidbody")) {
        std::unique_ptr<Command> cmd(new AddRigidbodyCmd(e->id));
        undo.Execute(std::move(cmd), scene);
      }
      if (!e->hasAudioSource && ImGui::MenuItem("Audio Source")) {
        std::unique_ptr<Command> cmd(new AddAudioSourceCmd(e->id));
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
  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputTextWithHint("##logFilter", "Filtrar mensagens...", consoleFilterBuf, sizeof(consoleFilterBuf));
  ImGui::SameLine();
  int levelFilter = consoleLevelFilter;
  if (ImGui::Combo("##logLevel", &levelFilter, "Todos\0Info\0Sucesso\0Aviso\0Erro\0")) {
    consoleLevelFilter = levelFilter;
  }
  ImGui::SameLine();
  ImGui::Separator();
  const std::string needle = consoleFilterBuf;
  ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  for (size_t i = 0; i < log.Count(); ++i) {
    const LogEntry& entry = log.At(i);
    if (consoleLevelFilter == 1 && entry.level != LogLevel::Info) {
      continue;
    }
    if (consoleLevelFilter == 2 && entry.level != LogLevel::Success) {
      continue;
    }
    if (consoleLevelFilter == 3 && entry.level != LogLevel::Warning) {
      continue;
    }
    if (consoleLevelFilter == 4 && entry.level != LogLevel::Error) {
      continue;
    }
    if (!needle.empty() && entry.text.find(needle) == std::string::npos) {
      continue;
    }
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
static const char* MatNodeName(int type) {
  if (type == 1) {
    return "Value";
  }
  if (type == 2) {
    return "Texture";
  }
  if (type == 3) {
    return "Multiply";
  }
  if (type == 4) {
    return "Add";
  }
  if (type == 5) {
    return "Mix";
  }
  return "Color";
}
bool EditorApp::DrawMaterialNodes(MaterialData& mat) {
  bool matChanged = false;
  if (ImGui::Checkbox("Use Nodes", &mat.useNodes)) {
    matChanged = true;
  }
  if (!mat.useNodes) {
    return matChanged;
  }
  const char* addNames[6] = { "Color", "Value", "Texture", "Multiply", "Add", "Mix" };
  for (int t = 0; t < 6; ++t) {
    if (t > 0) {
      ImGui::SameLine();
    }
    char label[32];
    label[0] = '+';
    label[1] = 0;
    size_t li = 1;
    for (size_t ci = 0; addNames[t][ci] != 0 && li < 30; ++ci) {
      label[li++] = addNames[t][ci];
    }
    label[li] = 0;
    if (ImGui::SmallButton(label)) {
      MaterialNode nd;
      nd.id = mat.graph.nextId++;
      nd.type = t;
      nd.posX = 20.0f + static_cast<float>((mat.graph.nodes.size() % 4) * 170);
      nd.posY = 20.0f + static_cast<float>((mat.graph.nodes.size() / 4) * 120);
      mat.graph.nodes.push_back(nd);
      matChanged = true;
    }
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Delete Selected")) {
    for (size_t i = 0; i < mat.graph.nodes.size(); ++i) {
      if (mat.graph.nodes[i].id == nodeSelected) {
        mat.graph.nodes.erase(mat.graph.nodes.begin() + i);
        for (size_t j = 0; j < mat.graph.nodes.size(); ++j) {
          if (mat.graph.nodes[j].inA == nodeSelected) {
            mat.graph.nodes[j].inA = -1;
          }
          if (mat.graph.nodes[j].inB == nodeSelected) {
            mat.graph.nodes[j].inB = -1;
          }
          if (mat.graph.nodes[j].inC == nodeSelected) {
            mat.graph.nodes[j].inC = -1;
          }
        }
        if (mat.graph.outAlbedo == nodeSelected) {
          mat.graph.outAlbedo = -1;
        }
        if (mat.graph.outRoughness == nodeSelected) {
          mat.graph.outRoughness = -1;
        }
        if (mat.graph.outMetallic == nodeSelected) {
          mat.graph.outMetallic = -1;
        }
        if (mat.graph.outEmissive == nodeSelected) {
          mat.graph.outEmissive = -1;
        }
        nodeSelected = -1;
        matChanged = true;
        break;
      }
    }
  }
  std::string outItems = "None";
  for (size_t i = 0; i < mat.graph.nodes.size(); ++i) {
    char nb[32];
    std::snprintf(nb, sizeof(nb), "%d:%s", mat.graph.nodes[i].id, MatNodeName(mat.graph.nodes[i].type));
    outItems += std::string("\0") + nb;
  }
  int outs[4] = { mat.graph.outAlbedo, mat.graph.outRoughness, mat.graph.outMetallic, mat.graph.outEmissive };
  const char* outLabels[4] = { "Out Albedo", "Out Rough", "Out Metal", "Out Emissive" };
  for (int o = 0; o < 4; ++o) {
    int cur = 0;
    for (size_t i = 0; i < mat.graph.nodes.size(); ++i) {
      if (mat.graph.nodes[i].id == outs[o]) {
        cur = static_cast<int>(i) + 1;
        break;
      }
    }
    if (ImGui::Combo(outLabels[o], &cur, outItems.c_str())) {
      int nid = -1;
      if (cur >= 1 && static_cast<size_t>(cur - 1) < mat.graph.nodes.size()) {
        nid = mat.graph.nodes[static_cast<size_t>(cur - 1)].id;
      }
      if (o == 0) {
        mat.graph.outAlbedo = nid;
      } else if (o == 1) {
        mat.graph.outRoughness = nid;
      } else if (o == 2) {
        mat.graph.outMetallic = nid;
      } else {
        mat.graph.outEmissive = nid;
      }
      matChanged = true;
    }
  }
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 380.0f);
  ImGui::InvisibleButton("nodecanvas", canvasSize);
  bool canvasHover = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(18, 18, 24, 255));
  dl->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(60, 60, 70, 255));
  ImVec2 mouse = ImGui::GetMousePos();
  if (nodeLinkFrom >= 0 && ImGui::IsMouseClicked(1) && canvasHover) {
    nodeLinkFrom = -1;
  }
  if (nodeDragId < 0 && nodeLinkFrom < 0 && ImGui::IsMouseClicked(0) && canvasHover) {
    nodeSelected = -1;
  }
  for (size_t i = 0; i < mat.graph.nodes.size(); ++i) {
    MaterialNode& nd = mat.graph.nodes[i];
    float nw = 150.0f;
    float nh = 76.0f;
    if (nd.type == 2) {
      nh = 100.0f;
    }
    ImVec2 p0 = ImVec2(canvasPos.x + nd.posX, canvasPos.y + nd.posY);
    ImVec2 p1 = ImVec2(p0.x + nw, p0.y + nh);
    bool sel = nd.id == nodeSelected;
    dl->AddRectFilled(p0, p1, sel ? IM_COL32(50, 70, 110, 255) : IM_COL32(36, 36, 46, 255), 4.0f);
    dl->AddRect(p0, p1, nodeLinkFrom == nd.id ? IM_COL32(255, 200, 80, 255) : IM_COL32(90, 90, 110, 255), 4.0f);
    dl->AddText(ImVec2(p0.x + 8, p0.y + 4), IM_COL32(230, 230, 240, 255), MatNodeName(nd.type));
    ImVec2 inA = ImVec2(p0.x, p0.y + 30);
    ImVec2 inB = ImVec2(p0.x, p0.y + 48);
    ImVec2 outP = ImVec2(p1.x, p0.y + 30);
    bool hasIn = nd.type == 3 || nd.type == 4 || nd.type == 5;
    if (hasIn) {
      dl->AddCircleFilled(inA, 5.0f, nd.inA >= 0 ? IM_COL32(120, 220, 120, 255) : IM_COL32(90, 90, 100, 255));
      dl->AddCircleFilled(inB, 5.0f, nd.inB >= 0 ? IM_COL32(120, 220, 120, 255) : IM_COL32(90, 90, 100, 255));
    }
    dl->AddCircleFilled(outP, 5.0f, IM_COL32(230, 180, 90, 255));
    bool inBody = mouse.x >= p0.x && mouse.x <= p1.x && mouse.y >= p0.y && mouse.y <= p1.y;
    auto nearPin = [&mouse](ImVec2 pp) {
      float dx = mouse.x - pp.x;
      float dy = mouse.y - pp.y;
      return dx * dx + dy * dy <= 100.0f;
    };
    if (canvasHover && ImGui::IsMouseClicked(0)) {
      if (nearPin(outP)) {
        nodeLinkFrom = nd.id;
        nodeDragId = -1;
      } else if (hasIn && (nearPin(inA) || nearPin(inB))) {
        if (nodeLinkFrom >= 0 && nodeLinkFrom != nd.id) {
          bool toA = nearPin(inA) && (nd.inA < 0 || !nearPin(inB));
          if (nd.type == 5 && nd.inA >= 0 && nd.inB >= 0) {
            nd.inC = nodeLinkFrom;
          } else if (toA || nd.type == 5) {
            if (toA) {
              nd.inA = nodeLinkFrom;
            } else {
              nd.inB = nodeLinkFrom;
            }
          } else {
            nd.inB = nodeLinkFrom;
          }
          nodeLinkFrom = -1;
          matChanged = true;
        }
      } else if (inBody) {
        nodeSelected = nd.id;
        nodeDragId = nd.id;
        nodeDragDX = mouse.x - p0.x;
        nodeDragDY = mouse.y - p0.y;
      }
    }
    if (nodeDragId == nd.id) {
      if (ImGui::IsMouseDown(0)) {
        nd.posX = mouse.x - canvasPos.x - nodeDragDX;
        nd.posY = mouse.y - canvasPos.y - nodeDragDY;
        if (nd.posX < 0.0f) {
          nd.posX = 0.0f;
        }
        if (nd.posY < 0.0f) {
          nd.posY = 0.0f;
        }
        matChanged = true;
      } else {
        nodeDragId = -1;
      }
    }
    if (nodeLinkFrom == nd.id) {
      dl->AddLine(outP, mouse, IM_COL32(255, 200, 80, 255), 2.0f);
    }
    if (nd.inA >= 0) {
      for (size_t j = 0; j < mat.graph.nodes.size(); ++j) {
        if (mat.graph.nodes[j].id == nd.inA) {
          ImVec2 sp = ImVec2(canvasPos.x + mat.graph.nodes[j].posX + 150.0f, canvasPos.y + mat.graph.nodes[j].posY + 30);
          dl->AddBezierCubic(sp, ImVec2(sp.x + 30, sp.y), inA, ImVec2(inA.x - 30, inA.y), IM_COL32(120, 220, 120, 255), 2.0f, 12);
          break;
        }
      }
    }
    if (nd.inB >= 0) {
      for (size_t j = 0; j < mat.graph.nodes.size(); ++j) {
        if (mat.graph.nodes[j].id == nd.inB) {
          ImVec2 sp = ImVec2(canvasPos.x + mat.graph.nodes[j].posX + 150.0f, canvasPos.y + mat.graph.nodes[j].posY + 30);
          dl->AddBezierCubic(sp, ImVec2(sp.x + 30, sp.y), inB, ImVec2(inB.x - 30, inB.y), IM_COL32(120, 220, 120, 255), 2.0f, 12);
          break;
        }
      }
    }
  }
  if (nodeSelected >= 0) {
    for (size_t i = 0; i < mat.graph.nodes.size(); ++i) {
      MaterialNode& nd = mat.graph.nodes[i];
      if (nd.id != nodeSelected) {
        continue;
      }
      char title[48];
      std::snprintf(title, sizeof(title), "Node %d (%s)", nd.id, MatNodeName(nd.type));
      ImGui::Text("%s", title);
      if (nd.type == 0) {
        if (ImGui::ColorEdit3("Node Color", nd.color)) {
          matChanged = true;
        }
      } else if (nd.type == 1 || nd.type == 5) {
        if (ImGui::DragFloat("Node Value", &nd.value, 0.01f, 0.0f, 1.0f)) {
          matChanged = true;
        }
      } else if (nd.type == 2) {
        char tbuf[256];
        std::strncpy(tbuf, nd.texGuid.c_str(), sizeof(tbuf) - 1);
        tbuf[sizeof(tbuf) - 1] = 0;
        if (ImGui::InputText("Node Tex", tbuf, sizeof(tbuf))) {
          nd.texGuid = tbuf;
          matChanged = true;
        }
      }
      break;
    }
  }
  ImGui::Checkbox("Show HLSL", &nodeShowHlsl);
  if (nodeShowHlsl) {
    std::string hlsl;
    EmitMaterialGraphHlsl(mat.graph, hlsl);
    ImGui::TextUnformatted(hlsl.empty() ? "(empty graph)" : hlsl.c_str());
  }
  return matChanged;
}
bool EditorApp::DrawMaterialScalars(MaterialData& mat) {
  bool matChanged = false;    if (ImGui::ColorEdit3("Albedo", mat.albedo)) {
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
    if (ImGui::SliderInt("Link Mask", &mat.linkMask, 0, 255)) {
      matChanged = true;
    }
    if (ImGui::CollapsingHeader("Detail Maps")) {
      if (ImGui::DragFloat("Detail Tiling", &mat.detailTiling, 0.1f, 0.1f, 64.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Layered Material")) {
      if (ImGui::SliderFloat("Layer Weight", &mat.layerWeight, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::ColorEdit3("Layer Albedo", mat.layerAlbedo)) {
        matChanged = true;
      }
      if (ImGui::SliderFloat("Layer Roughness", &mat.layerRoughness, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::SliderFloat("Layer Metallic", &mat.layerMetallic, 0.0f, 1.0f)) {
        matChanged = true;
      }
      if (ImGui::Checkbox("Height Blend", &mat.layerHeightBlend)) {
        matChanged = true;
      }
      if (mat.layerHeightBlend && ImGui::SliderFloat("Layer Height", &mat.layerHeight, 0.0f, 1.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Subsurface Scattering")) {
      int kind = mat.sssKind < 0 ? 0 : (mat.sssKind > 3 ? 3 : mat.sssKind);
      if (ImGui::Combo("SSS Kind", &kind, "Off\0Skin\0Hair\0Eye\0")) {
        mat.sssKind = kind;
        matChanged = true;
      }
      if (ImGui::ColorEdit3("SSS Color", mat.sssColor)) {
        matChanged = true;
      }
      if (ImGui::SliderFloat("SSS Intensity", &mat.sssIntensity, 0.0f, 1.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Virtual Texture")) {
      if (ImGui::Checkbox("Virtualize Albedo", &mat.vtAlbedo)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Procedural")) {
      if (ImGui::Checkbox("Triplanar", &mat.triplanar)) {
        matChanged = true;
      }
      if (mat.triplanar && ImGui::DragFloat("Triplanar Scale", &mat.triplanarScale, 0.01f, 0.001f, 8.0f)) {
        matChanged = true;
      }
    }
    if (ImGui::CollapsingHeader("Node Graph")) {
      if (DrawMaterialNodes(mat)) {
        matChanged = true;
      }
    }
  return matChanged;
}

void EditorApp::DrawMaterialSection(Entity* e) {
  if (e == nullptr || !e->hasMaterial) {
    return;
  }
  {
    float sw[3] = { e->material.albedo[0], e->material.albedo[1], e->material.albedo[2] };
    ImGui::ColorButton("##matswatch", ImVec4(sw[0], sw[1], sw[2], 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(96, 48));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextDisabled("M %.2f  R %.2f", e->material.metallic, e->material.roughness);
    ImGui::TextDisabled("E %.2f  Link %d", e->material.emissiveStrength, e->material.linkMask);
    ImGui::EndGroup();
    const char* slotNames[5] = { "Albedo", "Normal", "ORM", "Height", "Emissive" };
    const std::string* slotGuids[5] = { &e->material.albedoTex, &e->material.normalTex, &e->material.ormTex, &e->material.heightTex, &e->material.emissiveTex };
    for (int s = 0; s < 5; ++s) {
      if (slotGuids[s]->empty()) {
        continue;
      }
      const TextureRecord* tr = assets.GetTexByGuid(*slotGuids[s]);
      if (tr == nullptr || tr->gpu == 0 || rhi == nullptr) {
        continue;
      }
      void* srv = rhi->GetTextureSRV(tr->gpu);
      if (srv == nullptr) {
        continue;
      }
      ImGui::Image(reinterpret_cast<ImTextureID>(srv), ImVec2(48, 48));
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", slotNames[s]);
      }
      ImGui::SameLine();
    }
    ImGui::NewLine();
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
  DrawMaterialSlot(e, "Detail", 17);
  DrawMaterialSlot(e, "Detail Normal", 18);
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
  } else if (slot == 17) {
    return &m.detailAlbedoTex;
  } else if (slot == 18) {
    return &m.detailNormalTex;
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
  std::string root = assets.AssetsDir();
  if (browserPath.empty() || browserPath.size() < root.size() || browserPath.compare(0, root.size(), root) != 0) {
    browserPath = root;
  }
  ImGui::InputText("Search", browserSearch, sizeof(browserSearch));
  ImGui::SameLine();
  int bf = browserFilter < 0 ? 0 : (browserFilter > 4 ? 4 : browserFilter);
  if (ImGui::Combo("Type", &bf, "All\0Meshes\0Textures\0Scenes\0Sources\0")) {
    browserFilter = bf;
  }
  ImGui::SameLine();
  if (ImGui::Button("Create +")) {
    ImGui::OpenPopup("BrowserCreate");
  }
  {
    size_t pend = assets.PendingImports();
    if (pend > 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("Indexando %u...", static_cast<unsigned>(pend));
    } else {
      ImGui::SameLine();
      ImGui::TextDisabled("%u meshes, %u tex", static_cast<unsigned>(assets.AllGuids().size()), static_cast<unsigned>(assets.AllTexGuids().size()));
    }
  }
  if (ImGui::BeginPopup("BrowserCreate")) {
    if (ImGui::MenuItem("New Folder")) {
      std::error_code cec;
      std::string np = (std::filesystem::path(browserPath) / "New Folder").string();
      int suffix = 1;
      while (std::filesystem::exists(np, cec) && !cec && suffix < 100) {
        ++suffix;
        np = (std::filesystem::path(browserPath) / ("New Folder " + std::to_string(suffix))).string();
      }
      if (std::filesystem::create_directory(np, cec) || !cec) {
        log.Add(LogLevel::Success, std::string("Folder created: ") + np);
      } else {
        Announce(LogLevel::Error, std::string("Could not create folder: ") + np);
      }
      assets.Scan();
    }
    if (ImGui::MenuItem("Import Model...")) {
      std::string picked;
      if (ShowOpenGltfDialog(window.NativeHandle(), picked)) {
        std::error_code cec;
        std::string dst = (std::filesystem::path(browserPath) / std::filesystem::path(picked).filename()).string();
        int suffix = 1;
        while (std::filesystem::exists(dst, cec) && !cec && suffix < 100) {
          std::string stem = std::filesystem::path(picked).stem().string();
          std::string ext = std::filesystem::path(picked).extension().string();
          dst = (std::filesystem::path(browserPath) / (stem + "_" + std::to_string(suffix++) + ext)).string();
        }
        std::filesystem::copy_file(picked, dst, cec);
        if (cec) {
          Announce(LogLevel::Error, std::string("Import copy failed: ") + picked);
        } else {
          log.Add(LogLevel::Success, std::string("Importing: ") + dst);
          assets.Scan();
        }
      }
    }
    if (ImGui::MenuItem("Import Texture...")) {
      std::string picked;
      if (ShowOpenImageDialog(window.NativeHandle(), picked)) {
        std::error_code cec;
        std::string dst = (std::filesystem::path(browserPath) / std::filesystem::path(picked).filename()).string();
        int suffix = 1;
        while (std::filesystem::exists(dst, cec) && !cec && suffix < 100) {
          std::string stem = std::filesystem::path(picked).stem().string();
          std::string ext = std::filesystem::path(picked).extension().string();
          dst = (std::filesystem::path(browserPath) / (stem + "_" + std::to_string(suffix++) + ext)).string();
        }
        std::filesystem::copy_file(picked, dst, cec);
        if (cec) {
          Announce(LogLevel::Error, std::string("Import copy failed: ") + picked);
        } else {
          log.Add(LogLevel::Success, std::string("Importing: ") + dst);
          assets.Scan();
        }
      }
    }
    ImGui::EndPopup();
  }
  auto toLower = [](std::string v) -> std::string {
    return Kizuri::Str::ToLowerCopy(v);
  };
  auto nameMatches = [this, &toLower](const std::string& fn) -> bool {
    if (browserSearch[0] == '\0') {
      return true;
    }
    return toLower(fn).find(toLower(browserSearch)) != std::string::npos;
  };
  auto lowerExt = [&toLower](const std::string& p) -> std::string {
    return toLower(std::filesystem::path(p).extension().string());
  };
  auto shortName = [](const std::string& fn) -> std::string {
    if (fn.size() > 26) {
      return fn.substr(0, 23) + "...";
    }
    return fn;
  };
  auto stemName = [](const std::string& fn) -> std::string {
    size_t dot = fn.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? fn : fn.substr(0, dot);
    if (stem.size() > 26) {
      return stem.substr(0, 23) + "...";
    }
    return stem;
  };
  auto loadHdrSky = [this](const std::string& path, const std::string& fn) {
    Kizuri::RHITexture sky = assets.ImportHdrSky(path);
    if (sky == 0) {
      Announce(LogLevel::Error, std::string("HDR sky failed: ") + fn);
    } else if (!renderer.UseHdrSky(sky)) {
      Announce(LogLevel::Error, "HDR sky adopt failed");
    } else {
      renderer.BakeIrradiance();
      renderer.SetHdrBackground(true);
      log.Add(LogLevel::Success, std::string("HDR sky loaded: ") + fn);
    }
  };
  auto loadCineLut = [this](const std::string& path, const std::string& fn) {
    std::vector<unsigned char> lutPx;
    int lutSize = 0;
    if (!Kizuri::ParseCubeLut(path, lutPx, lutSize)) {
      Announce(LogLevel::Error, std::string("LUT parse failed: ") + fn);
    } else if (!renderer.UploadLut(lutPx.data(), lutSize)) {
      Announce(LogLevel::Error, "LUT upload failed");
    } else {
      viewLutPreset = 5;
      viewLutAmount = 1.0f;
      renderer.SetLutAmount(1.0f);
      log.Add(LogLevel::Success, std::string("LUT loaded: ") + fn);
    }
  };
  float previewH = 150.0f;
  ImGui::BeginChild("BrTree", ImVec2(205.0f, -previewH), true);
  std::function<void(const std::string&, int)> drawTreeDir = [&](const std::string& dir, int depth) {
    if (depth > 8) {
      return;
    }
    std::error_code tec;
    std::vector<std::string> subs;
    for (std::filesystem::directory_iterator tit(dir, tec), tend; tit != tend && !tec; tit.increment(tec)) {
      std::error_code fec;
      if (tit->is_directory(fec) && !fec) {
        subs.push_back(tit->path().string());
      }
    }
    std::sort(subs.begin(), subs.end());
    for (size_t i = 0; i < subs.size(); ++i) {
      std::string fn = std::filesystem::path(subs[i]).filename().string();
      ImGui::PushID(static_cast<int>(400000 + depth * 1000 + i));
      bool open = browserPath == subs[i];
      if (open) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
      }
      bool node = ImGui::TreeNodeEx(fn.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (open ? ImGuiTreeNodeFlags_Selected : 0));
      if (open) {
        ImGui::PopStyleColor();
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        browserPath = subs[i];
        browserSelPath = subs[i];
      }
      if (node) {
        drawTreeDir(subs[i], depth + 1);
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  };
  {
    ImGui::PushID(399999);
    bool selRoot = browserPath == root;
    if (selRoot) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    }
    bool nodeRoot = ImGui::TreeNodeEx("Assets", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen | (selRoot ? ImGuiTreeNodeFlags_Selected : 0));
    if (selRoot) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      browserPath = root;
      browserSelPath = root;
    }
    if (nodeRoot) {
      drawTreeDir(root, 1);
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("BrMain", ImVec2(0, -previewH), true);
  {
    std::string rel = browserPath.size() > root.size() ? browserPath.substr(root.size()) : "";
    ImGui::PushID(399998);
    if (ImGui::SmallButton("Assets")) {
      browserPath = root;
    }
    std::filesystem::path accumP = std::filesystem::path(root);
    std::string tmp = rel;
    size_t pos = 0;
    int crumb = 0;
    while (pos < tmp.size() && crumb < 16) {
      while (pos < tmp.size() && (tmp[pos] == '/' || tmp[pos] == '\\')) {
        ++pos;
      }
      if (pos >= tmp.size()) {
        break;
      }
      size_t end = pos;
      while (end < tmp.size() && tmp[end] != '/' && tmp[end] != '\\') {
        ++end;
      }
      std::string part = tmp.substr(pos, end - pos);
      pos = end;
      accumP /= part;
      ImGui::SameLine();
      ImGui::PushID(399900 + crumb);
      if (ImGui::SmallButton(part.c_str())) {
        browserPath = accumP.string();
      }
      ImGui::PopID();
      ++crumb;
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  std::error_code bec;
  std::vector<std::string> folderDirs;
  std::vector<std::string> folderGlbs;
  std::vector<std::string> folderImages;
  std::vector<std::string> folderScenes;
  std::vector<std::string> folderMisc;
  for (std::filesystem::directory_iterator bit(browserPath, bec), bend; bit != bend && !bec; bit.increment(bec)) {
    std::error_code fec;
    bool isDir = bit->is_directory(fec);
    if (fec) {
      continue;
    }
    std::string fp = bit->path().string();
    std::string fn = bit->path().filename().string();
    if (!nameMatches(fn)) {
      continue;
    }
    if (isDir) {
      folderDirs.push_back(fp);
      continue;
    }
    std::string ext = lowerExt(fp);
    if (ext == ".glb" || ext == ".gltf") {
      folderGlbs.push_back(fp);
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
      folderImages.push_back(fp);
    } else if (ext == ".kzscene") {
      folderScenes.push_back(fp);
    } else if (ext == ".cube" || ext == ".hlsl") {
      folderMisc.push_back(fp);
    }
  }
  auto sortByName = [](std::vector<std::string>& v) {
    std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) {
      return std::filesystem::path(a).filename().string() < std::filesystem::path(b).filename().string();
    });
  };
  sortByName(folderDirs);
  sortByName(folderGlbs);
  sortByName(folderImages);
  sortByName(folderScenes);
  sortByName(folderMisc);
  std::vector<const MeshRecord*> folderMeshes;
  if (bf == 0 || bf == 1) {
    std::vector<std::string> allGuids = assets.AllGuids();
    for (size_t i = 0; i < allGuids.size(); ++i) {
      const MeshRecord* rec = assets.GetByGuid(allGuids[i]);
      if (rec == nullptr) {
        continue;
      }
      if (Kizuri::IsAutoLodPath(rec->meshPath)) {
        continue;
      }
      std::string parent = std::filesystem::path(rec->meshPath).parent_path().string();
      if (parent != browserPath) {
        continue;
      }
      std::string fn = std::filesystem::path(rec->meshPath).filename().string();
      if (!nameMatches(fn)) {
        continue;
      }
      folderMeshes.push_back(rec);
    }
    std::sort(folderMeshes.begin(), folderMeshes.end(), [](const MeshRecord* a, const MeshRecord* b) {
      return std::filesystem::path(a->meshPath).filename().string() < std::filesystem::path(b->meshPath).filename().string();
    });
  }
  std::vector<const TextureRecord*> folderTexs;
  if (bf == 0 || bf == 2) {
    std::vector<std::string> allTexGuids = assets.AllTexGuids();
    for (size_t i = 0; i < allTexGuids.size(); ++i) {
      const TextureRecord* rec = assets.GetTexByGuid(allTexGuids[i]);
      if (rec == nullptr) {
        continue;
      }
      std::string parent = std::filesystem::path(rec->sourcePath).parent_path().string();
      if (parent != browserPath) {
        continue;
      }
      std::string fn = std::filesystem::path(rec->sourcePath).filename().string();
      if (!nameMatches(fn)) {
        continue;
      }
      folderTexs.push_back(rec);
    }
    std::sort(folderTexs.begin(), folderTexs.end(), [](const TextureRecord* a, const TextureRecord* b) {
      return std::filesystem::path(a->sourcePath).filename().string() < std::filesystem::path(b->sourcePath).filename().string();
    });
  }
  float availX = ImGui::GetContentRegionAvail().x;
  int cols = availX > 130.0f ? static_cast<int>(availX / 120.0f) : 1;
  if (cols < 1) {
    cols = 1;
  }
  {
    auto normPath = [](const std::string& p) -> std::string {
      std::string o = p;
      for (size_t i = 0; i < o.size(); ++i) {
        if (o[i] == '\\') {
          o[i] = '/';
        }
      }
      return o;
    };
    std::vector<std::string> keptGlbs;
    for (size_t i = 0; i < folderGlbs.size(); ++i) {
      bool sourced = false;
      for (size_t k = 0; k < folderMeshes.size() && !sourced; ++k) {
        if (folderMeshes[k] != nullptr && folderMeshes[k]->hasSource && normPath(folderMeshes[k]->sourcePath) == normPath(folderGlbs[i])) {
          sourced = true;
        }
      }
      if (!sourced) {
        keptGlbs.push_back(folderGlbs[i]);
      }
    }
    folderGlbs.swap(keptGlbs);
    std::vector<std::string> keptImages;
    for (size_t i = 0; i < folderImages.size(); ++i) {
      bool sourced = false;
      for (size_t k = 0; k < folderTexs.size() && !sourced; ++k) {
        if (folderTexs[k] != nullptr && normPath(folderTexs[k]->sourcePath) == normPath(folderImages[i])) {
          sourced = true;
        }
      }
      if (!sourced) {
        keptImages.push_back(folderImages[i]);
      }
    }
    folderImages.swap(keptImages);
  }
  int gridIdx = 0;
  auto gridNext = [&gridIdx, cols]() {
    ++gridIdx;
    if (gridIdx % cols != 0) {
      ImGui::SameLine();
    }
  };
  auto meshTag = [](const MeshRecord* rec) -> const char* {
    if (rec->state == MeshAssetState::Outdated) {
      return "[OUT]";
    } else if (rec->state == MeshAssetState::NoSource) {
      return "[NOSRC]";
    } else if (rec->state == MeshAssetState::SourceMissing) {
      return "[MISS]";
    } else if (!rec->loaded) {
      return "[LOAD]";
    }
    return "";
  };
  struct BrEntry {
    int kind;
    std::string path;
    const MeshRecord* mesh;
    const TextureRecord* tex;
  };
  std::vector<BrEntry> items;
  for (size_t i = 0; i < folderDirs.size(); ++i) {
    BrEntry e;
    e.kind = 0;
    e.path = folderDirs[i];
    e.mesh = nullptr;
    e.tex = nullptr;
    items.push_back(e);
  }
  for (size_t i = 0; i < folderMeshes.size(); ++i) {
    if (folderMeshes[i] == nullptr) {
      continue;
    }
    BrEntry e;
    e.kind = 1;
    e.path = folderMeshes[i]->meshPath;
    e.mesh = folderMeshes[i];
    e.tex = nullptr;
    items.push_back(e);
  }
  for (size_t i = 0; i < folderTexs.size(); ++i) {
    if (folderTexs[i] == nullptr) {
      continue;
    }
    BrEntry e;
    e.kind = 2;
    e.path = folderTexs[i]->sourcePath;
    e.mesh = nullptr;
    e.tex = folderTexs[i];
    items.push_back(e);
  }
  if (bf == 0 || bf == 4) {
    for (size_t i = 0; i < folderGlbs.size(); ++i) {
      BrEntry e;
      e.kind = 3;
      e.path = folderGlbs[i];
      e.mesh = nullptr;
      e.tex = nullptr;
      items.push_back(e);
    }
  }
  if (bf == 0 || bf == 3) {
    for (size_t i = 0; i < folderScenes.size(); ++i) {
      BrEntry e;
      e.kind = 4;
      e.path = folderScenes[i];
      e.mesh = nullptr;
      e.tex = nullptr;
      items.push_back(e);
    }
  }
  if (bf == 0 || bf == 2) {
    for (size_t i = 0; i < folderImages.size(); ++i) {
      bool knownTex = false;
      for (size_t k = 0; k < folderTexs.size(); ++k) {
        if (folderTexs[k] != nullptr && folderTexs[k]->sourcePath == folderImages[i]) {
          knownTex = true;
          break;
        }
      }
      if (knownTex) {
        continue;
      }
      BrEntry e;
      e.kind = 5;
      e.path = folderImages[i];
      e.mesh = nullptr;
      e.tex = nullptr;
      items.push_back(e);
    }
  }
  if (bf == 0 || bf == 4) {
    for (size_t i = 0; i < folderMisc.size(); ++i) {
      BrEntry e;
      e.kind = 6;
      e.path = folderMisc[i];
      e.mesh = nullptr;
      e.tex = nullptr;
      items.push_back(e);
    }
  }
  std::sort(items.begin(), items.end(), [](const BrEntry& a, const BrEntry& b) {
    if ((a.kind == 0) != (b.kind == 0)) {
      return a.kind == 0;
    }
    return std::filesystem::path(a.path).filename().string() < std::filesystem::path(b.path).filename().string();
  });
  int uid = 600000;
  for (size_t i = 0; i < items.size(); ++i) {
    const BrEntry& it = items[i];
    if (it.kind == 0) {
      std::string fn = std::filesystem::path(it.path).filename().string();
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.36f, 0.16f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.44f, 0.20f, 1.0f));
      ImGui::Button("Folder", ImVec2(96, 64));
      ImGui::PopStyleColor(2);
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        browserPath = it.path;
        browserSelPath = it.path;
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        browserSelPath = it.path;
      }
      ImGui::Text("%s", shortName(fn).c_str());
      ImGui::EndGroup();
      if (ImGui::BeginPopupContextItem("DirCtx")) {
        if (ImGui::MenuItem("Open")) {
          browserPath = it.path;
          browserSelPath = it.path;
        }
        if (ImGui::MenuItem("Delete Folder")) {
          std::error_code dec;
          bool empty = std::filesystem::is_empty(it.path, dec);
          if (!dec && empty) {
            std::filesystem::remove(it.path, dec);
            assets.Scan();
          } else {
            Announce(LogLevel::Warning, "Folder is not empty");
          }
        }
        ImGui::EndPopup();
      }
      ImGui::PopID();
    } else if (it.kind == 1 && it.mesh != nullptr) {
      const MeshRecord* rec = it.mesh;
      std::filesystem::path mp(rec->meshPath);
      std::string fn = mp.filename().string();
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      bool sel = selectedAssetGuid == rec->guid && !selectedAssetIsTex;
      if (sel) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.65f, 1.0f));
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.32f, 0.52f, 1.0f));
      }
      ImGui::Button("Mesh", ImVec2(96, 64));
      ImGui::PopStyleColor();
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedAssetGuid = rec->guid;
        selectedAssetIsTex = false;
        browserSelPath = rec->meshPath;
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        CreateEntityAt(0.0f, 0.0f, 0.0f, rec->guid);
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("KZ_ASSET", rec->guid.c_str(), rec->guid.size() + 1);
        ImGui::Text("%s", fn.c_str());
        ImGui::EndDragDropSource();
      }
      ImGui::Text("%s", stemName(fn).c_str());
      const char* tag = meshTag(rec);
      if (tag[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%s", tag);
      }
      ImGui::EndGroup();
      if (ImGui::BeginPopupContextItem("MeshCtx")) {
        bool canReimport = rec->hasSource;
        if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
          if (assets.Reimport(rec->guid)) {
            Announce(LogLevel::Info, std::string("Reimporting ") + fn);
          } else {
            Announce(LogLevel::Warning, "No source to reimport from");
          }
        }
        if (ImGui::MenuItem("Rename")) {
          assetRenameActive = true;
          assetRenameIsTex = false;
          assetRenameGuid = rec->guid;
          std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
          assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
        }
        if (ImGui::MenuItem("Delete")) {
          std::vector<RefUse> refs = SceneMeshRefs();
          std::vector<std::string> blocked;
          if (assets.DeleteAssetFile(rec->guid, false, refs, blocked)) {
            Announce(LogLevel::Warning, std::string("Deleted ") + fn);
          } else {
            std::string msg = "Cannot delete '" + fn + "': used by " + std::to_string(blocked.size()) + " (";
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
    } else if (it.kind == 2 && it.tex != nullptr) {
      const TextureRecord* rec = it.tex;
      std::filesystem::path tp(rec->sourcePath);
      std::string fn = tp.filename().string();
      void* thumbSrv = nullptr;
      if (rec->gpu != 0 && rhi != nullptr) {
        thumbSrv = rhi->GetTextureSRV(rec->gpu);
      }
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      bool sel = selectedAssetGuid == rec->guid && selectedAssetIsTex;
      bool pressed = false;
      if (thumbSrv != nullptr) {
        if (sel) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.65f, 1.0f));
        }
        pressed = ImGui::ImageButton("tx", reinterpret_cast<ImTextureID>(thumbSrv), ImVec2(96, 64));
        if (sel) {
          ImGui::PopStyleColor();
        }
      } else {
        if (sel) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.65f, 1.0f));
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.42f, 0.28f, 1.0f));
        }
        pressed = ImGui::Button("Tex", ImVec2(96, 64));
        ImGui::PopStyleColor();
      }
      (void)pressed;
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedAssetGuid = rec->guid;
        selectedAssetIsTex = true;
        browserSelPath = rec->sourcePath;
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("KZ_TEX", rec->guid.c_str(), rec->guid.size() + 1);
        ImGui::Text("%s", fn.c_str());
        ImGui::EndDragDropSource();
      }
      ImGui::Text("%s", stemName(fn).c_str());
      ImGui::EndGroup();
      if (ImGui::BeginPopupContextItem("TexCtx")) {
        bool canReimport = rec->hasSource;
        if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
          if (assets.ReimportTexture(rec->guid)) {
            Announce(LogLevel::Info, std::string("Reimporting ") + fn);
          } else {
            Announce(LogLevel::Warning, "No source to reimport from");
          }
        }
        if (ImGui::MenuItem("Rename")) {
          assetRenameActive = true;
          assetRenameIsTex = true;
          assetRenameGuid = rec->guid;
          std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
          assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
        }
        if (ImGui::MenuItem("Delete")) {
          std::vector<std::string> blocked;
          if (assets.DeleteTextureFile(rec->guid, false, blocked)) {
            Announce(LogLevel::Warning, std::string("Deleted ") + fn);
          } else {
            std::string msg = "Cannot delete '" + fn + "': used by " + std::to_string(blocked.size()) + " (";
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
    } else if (it.kind == 3) {
      std::string fn = std::filesystem::path(it.path).filename().string();
      std::filesystem::path sib = std::filesystem::path(it.path).parent_path() / (std::filesystem::path(it.path).stem().string() + ".kzmesh");
      std::error_code sec;
      bool ready = std::filesystem::exists(sib, sec) && !sec;
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.24f, 0.14f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.30f, 0.18f, 1.0f));
      ImGui::Button("Source", ImVec2(96, 64));
      ImGui::PopStyleColor(2);
      ImGui::Text("%s", stemName(fn).c_str());
      ImGui::TextDisabled("%s", ready ? "[READY]" : "[PENDING]");
      ImGui::EndGroup();
      ImGui::PopID();
    } else if (it.kind == 4) {
      std::string fn = std::filesystem::path(it.path).filename().string();
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.22f, 0.48f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.28f, 0.58f, 1.0f));
      ImGui::Button("Scene", ImVec2(96, 64));
      ImGui::PopStyleColor(2);
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        DoOpenPath(it.path);
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        browserSelPath = it.path;
      }
      ImGui::Text("%s", shortName(fn).c_str());
      ImGui::EndGroup();
      ImGui::PopID();
    } else if (it.kind == 5) {
      std::string fn = std::filesystem::path(it.path).filename().string();
      std::string ext = Kizuri::Path::ExtensionLower(it.path);
      bool isHdr = ext == ".hdr";
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.30f, 0.55f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.38f, 0.66f, 1.0f));
      ImGui::Button(isHdr ? "HDR Sky" : "Image", ImVec2(96, 64));
      ImGui::PopStyleColor(2);
      if (isHdr && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        loadHdrSky(it.path, fn);
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        browserSelPath = it.path;
      }
      ImGui::Text("%s", shortName(fn).c_str());
      ImGui::EndGroup();
      ImGui::PopID();
    } else if (it.kind == 6) {
      std::string fn = std::filesystem::path(it.path).filename().string();
      std::string ext6 = Kizuri::Path::ExtensionLower(it.path);
      bool isCube = ext6 == ".cube";
      bool isShader = ext6 == ".hlsl";
      ImGui::PushID(uid++);
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.32f, 0.34f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.40f, 0.42f, 1.0f));
      ImGui::Button(isCube ? "LUT" : (isShader ? "Shader" : "File"), ImVec2(96, 64));
      ImGui::PopStyleColor(2);
      if (isCube && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        loadCineLut(it.path, fn);
      }
      if (isShader && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ShellExecuteW(nullptr, L"open", std::filesystem::path(it.path).wstring().c_str(), nullptr, nullptr, SW_SHOW);
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        browserSelPath = it.path;
      }
      ImGui::Text("%s", shortName(fn).c_str());
      ImGui::EndGroup();
      ImGui::PopID();
    }
    gridNext();
  }
  ImGui::EndChild();
  ImGui::BeginChild("BrPreview", ImVec2(0, 0), true);
  {
    const MeshRecord* pm = !selectedAssetGuid.empty() && !selectedAssetIsTex ? assets.GetByGuid(selectedAssetGuid) : nullptr;
    const TextureRecord* pt = !selectedAssetGuid.empty() && selectedAssetIsTex ? assets.GetTexByGuid(selectedAssetGuid) : nullptr;
    if (pm != nullptr) {
      size_t verts = pm->data.positions.size() / 3;
      ImGui::Text("Mesh: %s", std::filesystem::path(pm->meshPath).filename().string().c_str());
      ImGui::TextDisabled("%llu verts  %llu tris  %llu parts", (unsigned long long)verts, (unsigned long long)(pm->data.indices.size() / 3), (unsigned long long)pm->data.parts.size());
      if (ImGui::Button("Add to Scene")) {
        CreateEntityAt(0.0f, 0.0f, 0.0f, pm->guid);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("double-click a tile also adds it");
    } else if (pt != nullptr) {
      if (pt->gpu != 0 && rhi != nullptr) {
        void* psrv = rhi->GetTextureSRV(pt->gpu);
        if (psrv != nullptr) {
          float avail = ImGui::GetContentRegionAvail().x;
          float side = avail > 110.0f ? 110.0f : avail;
          if (side > 32.0f) {
            ImGui::Image(reinterpret_cast<ImTextureID>(psrv), ImVec2(side, side));
            ImGui::SameLine();
          }
        }
      }
      ImGui::Text("Texture: %s", std::filesystem::path(pt->sourcePath).filename().string().c_str());
      ImGui::TextDisabled("%dx%d  %llu mip", pt->data.width, pt->data.height, (unsigned long long)pt->data.mips.size());
    } else if (!browserSelPath.empty()) {
      std::error_code pec;
      bool isDir = std::filesystem::is_directory(browserSelPath, pec) && !pec;
      std::string pext = Kizuri::Path::ExtensionLower(browserSelPath);
      std::string pfn = std::filesystem::path(browserSelPath).filename().string();
      ImGui::Text("%s", pfn.c_str());
      if (isDir) {
        if (ImGui::Button("Open Folder")) {
          browserPath = browserSelPath;
        }
      } else if (pext == ".hdr") {
        ImGui::TextDisabled("Double-click loads it as sky, or:");
        if (ImGui::Button("Set as Sky")) {
          loadHdrSky(browserSelPath, pfn);
        }
      } else if (pext == ".cube") {
        ImGui::TextDisabled("Color grading LUT, or:");
        if (ImGui::Button("Load LUT")) {
          loadCineLut(browserSelPath, pfn);
        }
      } else if (pext == ".kzscene") {
        if (ImGui::Button("Open Scene")) {
          DoOpenPath(browserSelPath);
        }
      } else if (pext == ".hlsl") {
        size_t lines = 0;
        {
          FILE* fp = nullptr;
          if (fopen_s(&fp, browserSelPath.c_str(), "rb") == 0 && fp != nullptr) {
            int ch = 0;
            while ((ch = std::fgetc(fp)) != EOF) {
              if (ch == '\n') {
                ++lines;
              }
            }
            std::fclose(fp);
          }
        }
        ImGui::TextDisabled("%llu lines  (double-click opens externally)", (unsigned long long)lines);
        if (ImGui::Button("Open Externally")) {
          ShellExecuteW(nullptr, L"open", std::filesystem::path(browserSelPath).wstring().c_str(), nullptr, nullptr, SW_SHOW);
        }
      } else if (pext == ".glb" || pext == ".gltf") {
        ImGui::TextDisabled("Importing runs in the background; the Mesh tile appears when ready.");
      } else {
        ImGui::TextDisabled("%s", browserSelPath.c_str());
      }
    } else {
      ImGui::TextDisabled("Select an asset: drag meshes into the viewport, double-click HDR/LUT/scene.");
    }
  }
  ImGui::EndChild();
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
