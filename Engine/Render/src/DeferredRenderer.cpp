#include "Kizuri/DeferredRenderer.h"
#include "Kizuri/Shadows.h"
#define A_CPU 1
#include "ffx_a.h"
#include "ffx_fsr1.h"
#include <cmath>
#include <cstring>
#include <string>
namespace Kizuri {
namespace {
struct GeoConstants {
  float world[16];
  float view[16];
  float proj[16];
};
struct LightConstants {
  float camPos[4];
  float lightDir[4];
  float lightColor[4];
  float counts[4];
  float camFwd[4];
  float lightA[16][4];
  float lightB[16][4];
  float lightC[16][4];
  float lightD[16][4];
  float cascadeVP[4][16];
  float cascadeSplit[4];
  float cascadeNear[4];
  float cascadeFar[4];
  float cascadeUV[4];
  float cascadeK[4];
  float cascadeSize[4];
  float cascadeLight[4];
  float shadowInfo[4];
  float spotVP[4][16];
  float spotMeta[4][4];
  float pointInfo[4];
  float pointMeta[4];
  float gradeInfo[4];
  float camRight[4];
  float camUp[4];
  float skySun[4];
  float skyColor[4];
  float ssaoInfo[4];
  float ssaoVP[16];
  float fogInfo[4];
  float fogColor[4];
};
struct MatConstants {
  float albedo[4];
  float params[4];
};
}
DeferredRenderer::DeferredRenderer()
  : rhi(nullptr)
  , w(0)
  , h(0)
  , gAlbedo(0)
  , gNormalRough(0)
  , gMetallic(0)
  , gPosition(0)
  , gDepth(0)
  , gViewport(0)
  , vb(0)
  , ib(0)
  , geoCB(0)
  , lightCB(0)
  , matCB(0)
  , geoVS(0)
  , geoPS(0)
  , lightVS(0)
  , lightPS(0)
  , depthVS(0)
  , shadowAtlas(0)
  , shadowCube(0)
  , shadowSampler(0)
  , shadowsAvailable(false)
  , shadowDebug(false)
  , shadowMapsValid(false)
  , shadowCascadeActive(0)
  , shadowSpotActive(0)
  , shadowPointFar(1.0f)
  , shadowPointNear(0.5f)
  , shadowPointSize(0.0f)
  , shadowPointLight(-1)
  , shadowPointActive(false)
  , bloomBrightPS(0)
  , bloomBlurPS(0)
  , bloomAddPS(0)
  , bloomCB(0)
  , gLight(0)
  , bloomA(0)
  , bloomB(0)
  , bloomW(1)
  , bloomH(1)
  , exposure(1.0f)
  , acesOn(true)
  , bloomStrength(0.5f)
  , fsrEasuPS(0)
  , fsrRcasPS(0)
  , fsrCB(0)
  , fsrA(0)
  , fsrOut(0)
  , outW(0)
  , outH(0)
  , renderScale(1.0f)
  , fsrSharpness(0.5f)
  , ssaoPS(0)
  , ssaoBlurPS(0)
  , ssaoCB(0)
  , ssaoA(0)
  , ssaoB(0)
  , ssaoW(1)
  , ssaoH(1)
  , ssaoOn(true)
  , ssaoIntensity(1.0f)
  , ssaoRadius(0.5f)
  , ssaoValid(false)
  , fogOn(true)
  , fogDensity(0.002f)
  , layout(0)
  , sampler(0)
  , indexCount(0)
  , viewX(0.0f)
  , viewY(0.0f)
  , begun(false)
  , ready(false) {
  material.albedo[0] = 0.8f;
  material.albedo[1] = 0.2f;
  fogColor[0] = 0.6f;
  fogColor[1] = 0.7f;
  fogColor[2] = 0.8f;
  material.albedo[2] = 0.15f;
  material.roughness = 0.5f;
  material.metallic = 0.0f;
  light.direction[0] = 0.4f;
  light.direction[1] = -1.0f;
  light.direction[2] = 0.3f;
  light.color[0] = 1.0f;
  light.color[1] = 1.0f;
  light.color[2] = 1.0f;
  light.intensity = 2.5f;
  std::memset(&mainCam, 0, sizeof(mainCam));
  std::memset(shadowCascadeVP, 0, sizeof(shadowCascadeVP));
  std::memset(shadowCascadeSplit, 0, sizeof(shadowCascadeSplit));
  std::memset(shadowCascadeNear, 0, sizeof(shadowCascadeNear));
  std::memset(shadowCascadeFar, 0, sizeof(shadowCascadeFar));
  std::memset(shadowCascadeUV, 0, sizeof(shadowCascadeUV));
  std::memset(shadowCascadeK, 0, sizeof(shadowCascadeK));
  std::memset(shadowCascadeSize, 0, sizeof(shadowCascadeSize));
  std::memset(shadowSpotVP, 0, sizeof(shadowSpotVP));
  for (int i = 0; i < 4; ++i) {
    shadowCascadeLight[i] = -1.0f;
    shadowSpotMeta[i][0] = -1.0f;
    shadowSpotMeta[i][1] = -1.0f;
    shadowSpotMeta[i][2] = 0.0f;
    shadowSpotMeta[i][3] = 0.0f;
  }
  shadowPointPos[0] = 0.0f;
  shadowPointPos[1] = 0.0f;
  shadowPointPos[2] = 0.0f;
}
DeferredRenderer::~DeferredRenderer() {
  Shutdown();
}
bool DeferredRenderer::Initialize(IRHI* r, int vw, int vh, const char* shaderDir) {
  if (r == nullptr || vw <= 0 || vh <= 0 || shaderDir == nullptr) {
    return false;
  }
  rhi = r;
  outW = vw;
  outH = vh;
  w = static_cast<int>(static_cast<float>(vw) * renderScale);
  h = static_cast<int>(static_cast<float>(vh) * renderScale);
  if (w < 1) {
    w = 1;
  }
  if (h < 1) {
    h = 1;
  }
  std::string dir(shaderDir);
  std::string geoVSPath = dir + "/GeometryVS.hlsl";
  std::string geoPSPath = dir + "/GeometryPS.hlsl";
  std::string lightVSPath = dir + "/LightingVS.hlsl";
  std::string lightPSPath = dir + "/LightingPS.hlsl";
  std::string depthVSPath = dir + "/DepthVS.hlsl";
  std::string bloomBrightPath = dir + "/BloomBrightPS.hlsl";
  std::string bloomBlurPath = dir + "/BloomBlurPS.hlsl";
  std::string bloomAddPath = dir + "/BloomAddPS.hlsl";
  std::string fsrEasuPath = dir + "/FsrEasuPS.hlsl";
  std::string fsrRcasPath = dir + "/FsrRcasPS.hlsl";
  std::string ssaoPath = dir + "/SsaoPS.hlsl";
  std::string ssaoBlurPath = dir + "/SsaoBlurPS.hlsl";
  geoVS = rhi->CreateVertexShaderFromFile(geoVSPath.c_str(), "main");
  geoPS = rhi->CreatePixelShaderFromFile(geoPSPath.c_str(), "main");
  lightVS = rhi->CreateVertexShaderFromFile(lightVSPath.c_str(), "main");
  lightPS = rhi->CreatePixelShaderFromFile(lightPSPath.c_str(), "main");
  depthVS = rhi->CreateVertexShaderFromFile(depthVSPath.c_str(), "main");
  bloomBrightPS = rhi->CreatePixelShaderFromFile(bloomBrightPath.c_str(), "main");
  bloomBlurPS = rhi->CreatePixelShaderFromFile(bloomBlurPath.c_str(), "main");
  bloomAddPS = rhi->CreatePixelShaderFromFile(bloomAddPath.c_str(), "main");
  if (geoVS == 0 || geoPS == 0 || lightVS == 0 || lightPS == 0 || depthVS == 0) {
    return false;
  }
  if (bloomBrightPS == 0 || bloomBlurPS == 0 || bloomAddPS == 0) {
    return false;
  }
  fsrEasuPS = rhi->CreatePixelShaderFromFile(fsrEasuPath.c_str(), "main");
  fsrRcasPS = rhi->CreatePixelShaderFromFile(fsrRcasPath.c_str(), "main");
  if (fsrEasuPS == 0 || fsrRcasPS == 0) {
    return false;
  }
  ssaoPS = rhi->CreatePixelShaderFromFile(ssaoPath.c_str(), "main");
  ssaoBlurPS = rhi->CreatePixelShaderFromFile(ssaoBlurPath.c_str(), "main");
  if (ssaoPS == 0 || ssaoBlurPS == 0) {
    return false;
  }
  layout = rhi->CreateInputLayoutPNUT(geoVS);
  if (layout == 0) {
    return false;
  }
  GeoConstants gc;
  std::memset(&gc, 0, sizeof(gc));
  LightConstants lc;
  std::memset(&lc, 0, sizeof(lc));
  MatConstants mc;
  std::memset(&mc, 0, sizeof(mc));
  float bloomInit[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  uint32_t fsrInit[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  float ssaoInit[24] = { 0.0f };
  geoCB = rhi->CreateConstantBuffer(sizeof(GeoConstants), &gc);
  lightCB = rhi->CreateConstantBuffer(sizeof(LightConstants), &lc);
  matCB = rhi->CreateConstantBuffer(sizeof(MatConstants), &mc);
  bloomCB = rhi->CreateConstantBuffer(sizeof(bloomInit), &bloomInit);
  fsrCB = rhi->CreateConstantBuffer(sizeof(fsrInit), &fsrInit);
  ssaoCB = rhi->CreateConstantBuffer(sizeof(ssaoInit), &ssaoInit);
  if (geoCB == 0 || lightCB == 0 || matCB == 0) {
    return false;
  }
  if (bloomCB == 0 || fsrCB == 0 || ssaoCB == 0) {
    return false;
  }
  sampler = rhi->CreateSamplerLinear();
  if (sampler == 0) {
    return false;
  }
  shadowAtlas = rhi->CreateRenderTarget(ShadowAtlasSize, ShadowAtlasSize, RHIFormat::R32_DEPTH);
  shadowCube = rhi->CreateShadowCube(ShadowCubeSize);
  shadowSampler = rhi->CreateSamplerShadow();
  shadowsAvailable = shadowAtlas != 0 && shadowCube != 0 && shadowSampler != 0;
  if (!CreateTargets()) {
    return false;
  }
  ready = true;
  return true;
}
void DeferredRenderer::Shutdown() {
  if (rhi == nullptr) {
    return;
  }
  DestroyTargets();
  if (shadowAtlas != 0) {
    rhi->DestroyRenderTarget(shadowAtlas);
    shadowAtlas = 0;
  }
  if (shadowCube != 0) {
    rhi->DestroyRenderTarget(shadowCube);
    shadowCube = 0;
  }
  shadowsAvailable = false;
  shadowMapsValid = false;
  if (vb != 0) {
    rhi->DestroyBuffer(vb);
    vb = 0;
  }
  if (ib != 0) {
    rhi->DestroyBuffer(ib);
    ib = 0;
  }
  if (geoCB != 0) {
    rhi->DestroyConstantBuffer(geoCB);
    geoCB = 0;
  }
  if (lightCB != 0) {
    rhi->DestroyConstantBuffer(lightCB);
    lightCB = 0;
  }
  if (matCB != 0) {
    rhi->DestroyConstantBuffer(matCB);
    matCB = 0;
  }
  if (bloomCB != 0) {
    rhi->DestroyConstantBuffer(bloomCB);
    bloomCB = 0;
  }
  if (fsrCB != 0) {
    rhi->DestroyConstantBuffer(fsrCB);
    fsrCB = 0;
  }
  if (ssaoCB != 0) {
    rhi->DestroyConstantBuffer(ssaoCB);
    ssaoCB = 0;
  }
  rhi = nullptr;
  ready = false;
}
bool DeferredRenderer::SetMesh(const float* positions, const float* normals, const float* uvs, size_t vertexCount, const uint32_t* indices, size_t idxCount, const float* tangents) {
  if (rhi == nullptr || positions == nullptr || normals == nullptr || indices == nullptr) {
    return false;
  }
  if (vertexCount == 0 || idxCount == 0) {
    return false;
  }
  std::vector<float> interleaved;
  interleaved.reserve(vertexCount * 12);
  for (size_t i = 0; i < vertexCount; ++i) {
    interleaved.push_back(positions[i * 3 + 0]);
    interleaved.push_back(positions[i * 3 + 1]);
    interleaved.push_back(positions[i * 3 + 2]);
    float nx = normals[i * 3 + 0];
    float ny = normals[i * 3 + 1];
    float nz = normals[i * 3 + 2];
    interleaved.push_back(nx);
    interleaved.push_back(ny);
    interleaved.push_back(nz);
    float u = 0.0f;
    float v = 0.0f;
    if (uvs != nullptr) {
      u = uvs[i * 2 + 0];
      v = uvs[i * 2 + 1];
    }
    interleaved.push_back(u);
    interleaved.push_back(v);
    float tx = 1.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    float tw = 1.0f;
    if (tangents != nullptr) {
      tx = tangents[i * 4 + 0];
      ty = tangents[i * 4 + 1];
      tz = tangents[i * 4 + 2];
      tw = tangents[i * 4 + 3];
    }
    float d = tx * nx + ty * ny + tz * nz;
    tx -= d * nx;
    ty -= d * ny;
    tz -= d * nz;
    interleaved.push_back(tx);
    interleaved.push_back(ty);
    interleaved.push_back(tz);
    interleaved.push_back(tw);
  }
  uint64_t vbSize = static_cast<uint64_t>(interleaved.size() * sizeof(float));
  uint64_t ibSize = static_cast<uint64_t>(idxCount * sizeof(uint32_t));
  RHIBuffer nvb = rhi->CreateBuffer(vbSize, 48, false, interleaved.data());
  RHIBuffer nib = rhi->CreateBuffer(ibSize, 4, true, indices);
  if (nvb == 0 || nib == 0) {
    return false;
  }
  if (vb != 0) {
    rhi->DestroyBuffer(vb);
  }
  if (ib != 0) {
    rhi->DestroyBuffer(ib);
  }
  vb = nvb;
  ib = nib;
  indexCount = static_cast<uint32_t>(idxCount);
  return true;
}
void DeferredRenderer::SetMaterial(const DeferredMaterial& mat) {
  material = mat;
}
void DeferredRenderer::SetLight(const DeferredLight& l) {
  light = l;
}
void DeferredRenderer::ClearLights() {
  pointLights.clear();
  spotLights.clear();
  dirLights.clear();
}
void DeferredRenderer::AddPointLight(const RenderPointLight& l) {
  if (pointLights.size() + spotLights.size() + dirLights.size() >= 16) {
    return;
  }
  pointLights.push_back(l);
}
void DeferredRenderer::AddSpotLight(const RenderSpotLight& l) {
  if (pointLights.size() + spotLights.size() + dirLights.size() >= 16) {
    return;
  }
  spotLights.push_back(l);
}
void DeferredRenderer::AddDirectionalLight(const RenderDirectionalLight& l) {
  if (pointLights.size() + spotLights.size() + dirLights.size() >= 16) {
    return;
  }
  dirLights.push_back(l);
}
void DeferredRenderer::SetMainCamera(const MainCameraSetup& setup) {
  mainCam = setup;
}
void DeferredRenderer::SetShadowDebug(bool debug) {
  shadowDebug = debug;
}
void DeferredRenderer::SetGrade(float e, bool aces) {
  exposure = e;
  if (exposure < 0.05f) {
    exposure = 0.05f;
  }
  if (exposure > 8.0f) {
    exposure = 8.0f;
  }
  acesOn = aces;
}
void DeferredRenderer::SetBloom(float strength) {
  bloomStrength = strength;
  if (bloomStrength < 0.0f) {
    bloomStrength = 0.0f;
  }
  if (bloomStrength > 2.0f) {
    bloomStrength = 2.0f;
  }
}
void DeferredRenderer::SetSsao(bool on, float intensity, float radius) {  ssaoOn = on;
  ssaoIntensity = intensity;
  if (ssaoIntensity < 0.0f) {
    ssaoIntensity = 0.0f;
  }
  if (ssaoIntensity > 2.0f) {
    ssaoIntensity = 2.0f;
  }
  ssaoRadius = radius;
  if (ssaoRadius < 0.1f) {
    ssaoRadius = 0.1f;
  }
  if (ssaoRadius > 2.0f) {
    ssaoRadius = 2.0f;
  }
}
void DeferredRenderer::SetFog(bool on, float density, const float color[3]) {
  fogOn = on;
  fogDensity = density;
  if (fogDensity < 0.0f) {
    fogDensity = 0.0f;
  }
  if (fogDensity > 0.05f) {
    fogDensity = 0.05f;
  }
  if (color != nullptr) {
    fogColor[0] = color[0];
    fogColor[1] = color[1];
    fogColor[2] = color[2];
  }
}
void DeferredRenderer::RenderShadowMaps() {
  shadowCascadeActive = 0;
  shadowSpotActive = 0;
  shadowPointActive = false;
  shadowPointLight = -1;
  shadowMapsValid = false;
  for (int i = 0; i < 4; ++i) {
    shadowCascadeLight[i] = -1.0f;
    shadowSpotMeta[i][0] = -1.0f;
    shadowSpotMeta[i][1] = -1.0f;
    shadowSpotMeta[i][2] = 0.0f;
    shadowSpotMeta[i][3] = 0.0f;
  }
  if (!shadowsAvailable || rhi == nullptr || shadowDraws.empty()) {
    return;
  }
  struct Candidate {
    bool isSpot;
    size_t index;
    float distSq;
    int emission;
  };
  Candidate cands[32];
  size_t candCount = 0;
  size_t emission = 0;
  for (size_t i = 0; i < pointLights.size() && emission < 16; ++i, ++emission) {
    if (!pointLights[i].castShadow) {
      continue;
    }
    if (candCount >= 32) {
      break;
    }
    float dx = pointLights[i].pos[0] - mainCam.camPos[0];
    float dy = pointLights[i].pos[1] - mainCam.camPos[1];
    float dz = pointLights[i].pos[2] - mainCam.camPos[2];
    cands[candCount].isSpot = false;
    cands[candCount].index = i;
    cands[candCount].distSq = dx * dx + dy * dy + dz * dz;
    cands[candCount].emission = static_cast<int>(emission);
    ++candCount;
  }
  for (size_t i = 0; i < spotLights.size() && emission < 16; ++i, ++emission) {
    if (!spotLights[i].castShadow) {
      continue;
    }
    if (candCount >= 32) {
      break;
    }
    float dx = spotLights[i].pos[0] - mainCam.camPos[0];
    float dy = spotLights[i].pos[1] - mainCam.camPos[1];
    float dz = spotLights[i].pos[2] - mainCam.camPos[2];
    cands[candCount].isSpot = true;
    cands[candCount].index = i;
    cands[candCount].distSq = dx * dx + dy * dy + dz * dz;
    cands[candCount].emission = static_cast<int>(emission);
    ++candCount;
  }
  for (size_t i = 0; i < candCount; ++i) {
    for (size_t j = i + 1; j < candCount; ++j) {
      if (cands[j].distSq < cands[i].distSq) {
        Candidate tmp = cands[i];
        cands[i] = cands[j];
        cands[j] = tmp;
      }
    }
  }
  int dirCascades[4] = { 0, 0, 0, 0 };
  int dirEmission[4] = { -1, -1, -1, -1 };
  size_t dirLightIdx[4] = { 0, 0, 0, 0 };
  int dirCount = 0;
  emission = pointLights.size() + spotLights.size();
  for (size_t i = 0; i < dirLights.size() && dirCount < 4; ++i) {
    if (!dirLights[i].castShadow) {
      continue;
    }
    int cc = dirLights[i].cascades;
    if (cc < 2) {
      cc = 2;
    }
    if (cc > 4) {
      cc = 4;
    }
    dirCascades[dirCount] = cc;
    dirEmission[dirCount] = (emission + i < 16) ? static_cast<int>(emission + i) : -1;
    dirLightIdx[dirCount] = i;
    ++dirCount;
  }
  size_t spotOrder[32];
  size_t spotKept = 0;
  size_t pointKept = static_cast<size_t>(-1);
  int pointEmission = -1;
  for (size_t i = 0; i < candCount; ++i) {
    if (cands[i].isSpot) {
      if (spotKept < 32) {
        spotOrder[spotKept++] = cands[i].index;
      }
    } else if (pointKept == static_cast<size_t>(-1)) {
      pointKept = cands[i].index;
      pointEmission = cands[i].emission;
    }
  }
  ShadowAtlasPlan plan = PlanShadowAtlas(dirCascades, dirCount, static_cast<int>(spotKept));
  size_t spotsMapped = static_cast<size_t>(plan.spotMapped);
  if (spotsMapped > spotKept) {
    spotsMapped = spotKept;
  }
  bool anyDir = false;
  for (int i = 0; i < 4; ++i) {
    if (plan.dirTileStart[i] >= 0) {
      anyDir = true;
    }
  }
  if (!anyDir && spotsMapped == 0 && pointKept == static_cast<size_t>(-1)) {
    return;
  }
  float identity[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  rhi->SetInputLayout(layout);
  rhi->SetVertexShader(depthVS);
  rhi->SetPixelShader(0);
  rhi->SetTopology(RHITopology::TriangleList);
  RHIRasterizer srs;
  srs.cull = RHICull::Back;
  srs.fill = RHIFill::Solid;
  srs.frontCCW = false;
  srs.slopeBias = 2.0f;
  rhi->SetRasterizerState(srs);
  RHIDepthStencil sds;
  sds.depthEnable = true;
  sds.depthWrite = true;
  rhi->SetDepthStencilState(sds);
  RHIBlend sblend;
  sblend.enable = false;
  rhi->SetBlendState(sblend);
  rhi->SetVertexConstantBuffer(0, geoCB);
  int slot = 0;
  for (int d = 0; d < dirCount; ++d) {
    int start = plan.dirTileStart[d];
    if (start < 0) {
      continue;
    }
    const RenderDirectionalLight& dl = dirLights[dirLightIdx[d]];
    int need = dirCascades[d];
    int res = dl.shadowSize;
    if (res != 512 && res != 1024 && res != 2048) {
      res = 1024;
    }
    float splits[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    ShadowSplitDepths(mainCam.nearZ, mainCam.farZ, need, dl.lambda, splits);
    for (int c = 0; c < need; ++c) {
      int tile = start + c;
      if (tile < 0 || tile >= 4) {
        continue;
      }
      float vp[16];
      float cn = 0.0f;
      float cf = 1.0f;
      float ce = 1.0f;
      ShadowSunMatrix(dl.dir, mainCam.camPos, mainCam.camFwd, mainCam.camRight, mainCam.camUp, mainCam.fovY, mainCam.aspect, splits[c], splits[c + 1], res, vp, cn, cf, ce);
      std::memcpy(shadowCascadeVP[tile], vp, sizeof(shadowCascadeVP[tile]));
      shadowCascadeSplit[tile] = splits[c + 1];
      shadowCascadeNear[tile] = cn;
      shadowCascadeFar[tile] = cf;
      float frac = static_cast<float>(res) / static_cast<float>(ShadowTileSize);
      shadowCascadeUV[tile] = PCSSOrthoUVScale(ce, frac);
      shadowCascadeK[tile] = frac;
      shadowCascadeSize[tile] = dl.effectiveSize;
      shadowCascadeLight[tile] = static_cast<float>(dirEmission[d]);
      rhi->SetRenderTargets(0, nullptr, shadowAtlas);
      RHIViewport tvp;
      tvp.x = static_cast<float>((tile % 2) * ShadowTileSize);
      tvp.y = static_cast<float>((tile / 2) * ShadowTileSize);
      tvp.w = static_cast<float>(res);
      tvp.h = static_cast<float>(res);
      tvp.minD = 0.0f;
      tvp.maxD = 1.0f;
      rhi->SetViewport(tvp);
      rhi->ClearDepth(shadowAtlas);
      for (size_t di = 0; di < shadowDraws.size(); ++di) {
        GeoConstants gc;
        std::memcpy(gc.world, shadowDraws[di].world, sizeof(gc.world));
        std::memcpy(gc.view, identity, sizeof(gc.view));
        std::memcpy(gc.proj, vp, sizeof(gc.proj));
        rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
        rhi->SetVertexBuffer(shadowDraws[di].vb, 0);
        rhi->SetIndexBuffer(shadowDraws[di].ib);
        rhi->DrawIndexed(shadowDraws[di].count, shadowDraws[di].start, 0);
      }
      ++slot;
    }
  }
  shadowCascadeActive = slot;
  size_t spotEmitBase = pointLights.size();
  for (size_t s = 0; s < spotsMapped && s < 4; ++s) {
    const RenderSpotLight& sl = spotLights[spotOrder[s]];
    int tile = plan.spotTiles[s];
    if (tile < 0 || tile >= 4) {
      continue;
    }
    float vp[16];
    ShadowSpotMatrix(sl.pos, sl.dir, sl.angle, sl.radius, vp);
    std::memcpy(shadowSpotVP[s], vp, sizeof(shadowSpotVP[s]));
    int res = sl.shadowSize;
    if (res != 512 && res != 1024 && res != 2048) {
      res = 1024;
    }
    float frac = static_cast<float>(res) / static_cast<float>(ShadowTileSize);
    shadowSpotMeta[s][0] = static_cast<float>(tile);
    size_t em = spotEmitBase + spotOrder[s];
    shadowSpotMeta[s][1] = em < 16 ? static_cast<float>(em) : -1.0f;
    shadowSpotMeta[s][2] = sl.effectiveSize;
    shadowSpotMeta[s][3] = frac;
    rhi->SetRenderTargets(0, nullptr, shadowAtlas);
    RHIViewport tvp;
    tvp.x = static_cast<float>((tile % 2) * ShadowTileSize);
    tvp.y = static_cast<float>((tile / 2) * ShadowTileSize);
    tvp.w = static_cast<float>(res);
    tvp.h = static_cast<float>(res);
    tvp.minD = 0.0f;
    tvp.maxD = 1.0f;
    rhi->SetViewport(tvp);
    rhi->ClearDepth(shadowAtlas);
    for (size_t di = 0; di < shadowDraws.size(); ++di) {
      GeoConstants gc;
      std::memcpy(gc.world, shadowDraws[di].world, sizeof(gc.world));
      std::memcpy(gc.view, identity, sizeof(gc.view));
      std::memcpy(gc.proj, vp, sizeof(gc.proj));
      rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
      rhi->SetVertexBuffer(shadowDraws[di].vb, 0);
      rhi->SetIndexBuffer(shadowDraws[di].ib);
      rhi->DrawIndexed(shadowDraws[di].count, shadowDraws[di].start, 0);
    }
  }
  shadowSpotActive = static_cast<int>(spotsMapped > 4 ? 4 : spotsMapped);
  if (pointKept != static_cast<size_t>(-1)) {
    const RenderPointLight& pl = pointLights[pointKept];
    float faces[6][16];
    ShadowPointFaces(pl.pos, pl.radius, faces);
    shadowPointPos[0] = pl.pos[0];
    shadowPointPos[1] = pl.pos[1];
    shadowPointPos[2] = pl.pos[2];
    shadowPointFar = pl.radius;
    shadowPointNear = 0.5f;
    shadowPointSize = pl.effectiveSize;
    shadowPointLight = pointEmission;
    shadowPointActive = true;
    for (int f = 0; f < 6; ++f) {
      rhi->SetShadowCubeFace(shadowCube, f);
      RHIViewport cvp;
      cvp.x = 0.0f;
      cvp.y = 0.0f;
      cvp.w = static_cast<float>(ShadowCubeSize);
      cvp.h = static_cast<float>(ShadowCubeSize);
      cvp.minD = 0.0f;
      cvp.maxD = 1.0f;
      rhi->SetViewport(cvp);
      rhi->ClearDepth(shadowCube);
      for (size_t di = 0; di < shadowDraws.size(); ++di) {
        GeoConstants gc;
        std::memcpy(gc.world, shadowDraws[di].world, sizeof(gc.world));
        std::memcpy(gc.view, identity, sizeof(gc.view));
        std::memcpy(gc.proj, faces[f], sizeof(gc.proj));
        rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
        rhi->SetVertexBuffer(shadowDraws[di].vb, 0);
        rhi->SetIndexBuffer(shadowDraws[di].ib);
        rhi->DrawIndexed(shadowDraws[di].count, shadowDraws[di].start, 0);
      }
    }
  }
  shadowMapsValid = shadowCascadeActive > 0 || shadowSpotActive > 0 || shadowPointActive;
}
bool DeferredRenderer::Resize(int nw, int nh) {
  if (nw <= 0 || nh <= 0) {
    return false;
  }
  outW = nw;
  outH = nh;
  w = static_cast<int>(static_cast<float>(nw) * renderScale);
  h = static_cast<int>(static_cast<float>(nh) * renderScale);
  if (w < 1) {
    w = 1;
  }
  if (h < 1) {
    h = 1;
  }
  DestroyTargets();
  return CreateTargets();
}
void DeferredRenderer::SetFsr(float scale, float sharpness) {
  float s = scale;
  if (s < 0.5f) {
    s = 0.5f;
  }
  if (s > 1.0f) {
    s = 1.0f;
  }
  float sh = sharpness;
  if (sh < 0.0f) {
    sh = 0.0f;
  }
  if (sh > 1.0f) {
    sh = 1.0f;
  }
  if (s == renderScale && sh == fsrSharpness) {
    return;
  }
  renderScale = s;
  fsrSharpness = sh;
  if (outW > 0 && outH > 0) {
    w = static_cast<int>(static_cast<float>(outW) * renderScale);
    h = static_cast<int>(static_cast<float>(outH) * renderScale);
    if (w < 1) {
      w = 1;
    }
    if (h < 1) {
      h = 1;
    }
    DestroyTargets();
    CreateTargets();
  }
}
void DeferredRenderer::SetViewOffset(float x, float y) {
  viewX = x;
  viewY = y;
}
void DeferredRenderer::Render(const float view[16], const float proj[16], const float camPos[3]) {
  RenderInternal(view, proj, camPos, false);
}
void DeferredRenderer::RenderToTexture(const float view[16], const float proj[16], const float camPos[3]) {
  RenderInternal(view, proj, camPos, true);
}
void* DeferredRenderer::GetViewportTexture() {
  if (rhi == nullptr) {
    return nullptr;
  }
  bool fsrActive = renderScale < 0.999f && fsrOut != 0;
  RHIRenderTarget rt = fsrActive ? fsrOut : gViewport;
  if (rt == 0) {
    return nullptr;
  }
  return rhi->GetRenderTargetSRV(rt);
}
void DeferredRenderer::BeginObjects(const float view[16], const float proj[16]) {
  begun = false;
  if (!ready || rhi == nullptr) {
    return;
  }
  std::memcpy(lastView, view, sizeof(lastView));
  std::memcpy(lastProj, proj, sizeof(lastProj));
  shadowDraws.clear();
  shadowMapsValid = false;
  ssaoValid = false;
  RHIRenderTarget mrts[4] = { gAlbedo, gNormalRough, gMetallic, gPosition };
  rhi->SetRenderTargets(4, mrts, gDepth);
  rhi->ClearRenderTarget(gAlbedo, 0.02f, 0.02f, 0.03f, 1.0f);
  rhi->ClearRenderTarget(gNormalRough, 0.5f, 0.5f, 1.0f, 1.0f);
  rhi->ClearRenderTarget(gMetallic, 0.0f, 0.0f, 0.0f, 1.0f);
  rhi->ClearRenderTarget(gPosition, 0.0f, 0.0f, 0.0f, 1.0f);
  rhi->ClearDepth(gDepth);
  RHIViewport vp;
  vp.x = viewX;
  vp.y = viewY;
  vp.w = static_cast<float>(w);
  vp.h = static_cast<float>(h);
  vp.minD = 0.0f;
  vp.maxD = 1.0f;
  rhi->SetViewport(vp);
  rhi->SetTopology(RHITopology::TriangleList);
  RHIRasterizer rs;
  rs.cull = RHICull::Back;
  rs.fill = RHIFill::Solid;
  rs.frontCCW = false;
  rs.slopeBias = 0.0f;
  rhi->SetRasterizerState(rs);
  RHIDepthStencil ds;
  ds.depthEnable = true;
  ds.depthWrite = true;
  rhi->SetDepthStencilState(ds);
  RHIBlend blend;
  blend.enable = false;
  rhi->SetBlendState(blend);
  rhi->SetInputLayout(layout);
  rhi->SetVertexShader(geoVS);
  rhi->SetPixelShader(geoPS);
  rhi->SetVertexBuffer(vb, 0);
  rhi->SetIndexBuffer(ib);
  rhi->SetVertexConstantBuffer(0, geoCB);
  MatConstants mc;
  mc.albedo[0] = material.albedo[0];
  mc.albedo[1] = material.albedo[1];
  mc.albedo[2] = material.albedo[2];
  mc.albedo[3] = 1.0f;
  mc.params[0] = material.roughness;
  mc.params[1] = material.metallic;
  mc.params[2] = 0.0f;
  mc.params[3] = 0.0f;
  rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  rhi->SetPixelConstantBuffer(0, matCB);
  begun = true;
}
void DeferredRenderer::DrawObject(const float world[16]) {
  if (!begun || vb == 0 || ib == 0 || world == nullptr) {
    return;
  }
  ShadowDrawItem item;
  std::memcpy(item.world, world, sizeof(item.world));
  item.vb = vb;
  item.ib = ib;
  item.start = 0;
  item.count = indexCount;
  shadowDraws.push_back(item);
  GeoConstants gc;
  std::memcpy(gc.world, world, sizeof(gc.world));
  std::memcpy(gc.view, lastView, sizeof(gc.view));
  std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
  rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
  rhi->DrawIndexed(indexCount, 0, 0);
}
void DeferredRenderer::DrawObjectEx(const float world[16], RHIBuffer vb, RHIBuffer ib, uint32_t start, uint32_t count, const DeferredMaterial* mat) {
  if (!begun || world == nullptr || vb == 0 || ib == 0 || count == 0) {
    return;
  }
  ShadowDrawItem item;
  std::memcpy(item.world, world, sizeof(item.world));
  item.vb = vb;
  item.ib = ib;
  item.start = start;
  item.count = count;
  shadowDraws.push_back(item);
  GeoConstants gc;
  std::memcpy(gc.world, world, sizeof(gc.world));
  std::memcpy(gc.view, lastView, sizeof(gc.view));
  std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
  rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
  if (mat != nullptr) {
    MatConstants mc;
    mc.albedo[0] = mat->albedo[0];
    mc.albedo[1] = mat->albedo[1];
    mc.albedo[2] = mat->albedo[2];
    mc.albedo[3] = 1.0f;
    mc.params[0] = mat->roughness;
    mc.params[1] = mat->metallic;
    mc.params[2] = 0.0f;
    mc.params[3] = 0.0f;
    rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  } else {
    MatConstants mc;
    mc.albedo[0] = material.albedo[0];
    mc.albedo[1] = material.albedo[1];
    mc.albedo[2] = material.albedo[2];
    mc.albedo[3] = 1.0f;
    mc.params[0] = material.roughness;
    mc.params[1] = material.metallic;
    mc.params[2] = 0.0f;
    mc.params[3] = 0.0f;
    rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  }
  rhi->SetVertexBuffer(vb, 0);
  rhi->SetIndexBuffer(ib);
  rhi->DrawIndexed(count, start, 0);
}
void DeferredRenderer::EndObjectsToTexture(const float camPos[3]) {
  EndInternal(camPos, true);
}
void DeferredRenderer::EndObjectsToBackbuffer(const float camPos[3]) {
  EndInternal(camPos, false);
}
void DeferredRenderer::RenderInternal(const float view[16], const float proj[16], const float camPos[3], bool toTexture) {
  BeginObjects(view, proj);
  float identity[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  DrawObject(identity);
  if (toTexture) {
    EndObjectsToTexture(camPos);
  } else {
    EndObjectsToBackbuffer(camPos);
  }
}
void DeferredRenderer::EndInternal(const float camPos[3], bool toTexture) {
  if (!begun) {
    return;
  }
  begun = false;
  RenderShadowMaps();
  float ssaoVP[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  for (int cc = 0; cc < 4; ++cc) {
    for (int rr = 0; rr < 4; ++rr) {
      float s = 0.0f;
      for (int kk = 0; kk < 4; ++kk) {
        s += lastView[cc * 4 + kk] * lastProj[kk * 4 + rr];
      }
      ssaoVP[cc * 4 + rr] = s;
    }
  }
  if (toTexture && ssaoOn && ssaoA != 0 && ssaoB != 0) {
    float sq[24] = { 0.0f };
    sq[0] = ssaoRadius;
    sq[1] = ssaoIntensity;
    sq[2] = 0.0f;
    sq[3] = 0.0f;
    sq[4] = camPos[0];
    sq[5] = camPos[1];
    sq[6] = camPos[2];
    sq[7] = 0.0f;
    std::memcpy(&sq[8], ssaoVP, sizeof(float) * 16);
    rhi->UpdateConstantBuffer(ssaoCB, &sq, sizeof(sq));
    rhi->SetPixelConstantBuffer(0, ssaoCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetPixelSampler(1, sampler);
    rhi->SetRenderTargets(1, &ssaoA, 0);
    RHIViewport svp;
    svp.x = 0.0f;
    svp.y = 0.0f;
    svp.w = static_cast<float>(ssaoW);
    svp.h = static_cast<float>(ssaoH);
    svp.minD = 0.0f;
    svp.maxD = 1.0f;
    rhi->SetViewport(svp);
    rhi->SetVertexShader(lightVS);
    rhi->SetPixelShader(ssaoPS);
    rhi->SetPixelTexture(0, gPosition);
    rhi->SetPixelTexture(1, gNormalRough);
    rhi->DrawFullscreenTriangle();
    float bh[4] = { 1.0f / static_cast<float>(ssaoW), 0.0f, ssaoRadius, 0.0f };
    rhi->UpdateConstantBuffer(ssaoCB, &bh, sizeof(bh));
    rhi->SetRenderTargets(1, &ssaoB, 0);
    rhi->SetPixelShader(ssaoBlurPS);
    rhi->SetPixelTexture(0, ssaoA);
    rhi->SetPixelTexture(1, gPosition);
    rhi->SetPixelTexture(2, gNormalRough);
    rhi->DrawFullscreenTriangle();
    float bv[4] = { 0.0f, 1.0f / static_cast<float>(ssaoH), ssaoRadius, 0.0f };
    rhi->UpdateConstantBuffer(ssaoCB, &bv, sizeof(bv));
    rhi->SetRenderTargets(1, &ssaoA, 0);
    rhi->SetPixelTexture(0, ssaoB);
    rhi->SetPixelTexture(1, gPosition);
    rhi->SetPixelTexture(2, gNormalRough);
    rhi->DrawFullscreenTriangle();
    ssaoValid = true;
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    rhi->SetPixelTexture(2, 0);
  }
  if (toTexture) {
    rhi->SetRenderTargets(1, &gLight, 0);
  } else {
    rhi->BindBackbuffer();
  }
  RHIViewport lvp;
  lvp.x = viewX;
  lvp.y = viewY;
  lvp.w = static_cast<float>(w);
  lvp.h = static_cast<float>(h);
  lvp.minD = 0.0f;
  lvp.maxD = 1.0f;
  rhi->SetViewport(lvp);
  RHIDepthStencil dsOff;
  dsOff.depthEnable = false;
  dsOff.depthWrite = false;
  rhi->SetDepthStencilState(dsOff);
  rhi->SetVertexShader(lightVS);
  rhi->SetPixelShader(lightPS);
  rhi->SetPixelTexture(0, gAlbedo);
  rhi->SetPixelTexture(1, gNormalRough);
  rhi->SetPixelTexture(2, gMetallic);
  rhi->SetPixelTexture(3, gPosition);
  rhi->SetPixelSampler(0, sampler);
  LightConstants lc;
  lc.camPos[0] = camPos[0];
  lc.camPos[1] = camPos[1];
  lc.camPos[2] = camPos[2];
  lc.camPos[3] = 1.0f;
  bool useFallback = dirLights.empty();
  lc.lightDir[0] = useFallback ? light.direction[0] : 0.0f;
  lc.lightDir[1] = useFallback ? light.direction[1] : 0.0f;
  lc.lightDir[2] = useFallback ? light.direction[2] : 0.0f;
  lc.lightDir[3] = 0.0f;
  lc.lightColor[0] = useFallback ? light.color[0] * light.intensity : 0.0f;
  lc.lightColor[1] = useFallback ? light.color[1] * light.intensity : 0.0f;
  lc.lightColor[2] = useFallback ? light.color[2] * light.intensity : 0.0f;
  lc.lightColor[3] = 1.0f;
  size_t total = pointLights.size() + spotLights.size() + dirLights.size();
  if (total > 16) {
    total = 16;
  }
  lc.counts[0] = static_cast<float>(total);
  lc.counts[1] = 0.0f;
  lc.counts[2] = 0.0f;
  lc.counts[3] = 0.0f;
  lc.camFwd[0] = mainCam.camFwd[0];
  lc.camFwd[1] = mainCam.camFwd[1];
  lc.camFwd[2] = mainCam.camFwd[2];
  lc.camFwd[3] = 0.0f;
  size_t li = 0;
  for (size_t i = 0; i < pointLights.size() && li < 16; ++i) {
    const RenderPointLight& pl = pointLights[i];
    lc.lightA[li][0] = pl.pos[0];
    lc.lightA[li][1] = pl.pos[1];
    lc.lightA[li][2] = pl.pos[2];
    lc.lightA[li][3] = 0.0f;
    lc.lightB[li][0] = 0.0f;
    lc.lightB[li][1] = 0.0f;
    lc.lightB[li][2] = 0.0f;
    lc.lightB[li][3] = pl.radius;
    lc.lightC[li][0] = pl.color[0] * pl.intensity;
    lc.lightC[li][1] = pl.color[1] * pl.intensity;
    lc.lightC[li][2] = pl.color[2] * pl.intensity;
    lc.lightC[li][3] = 0.0f;
    lc.lightD[li][0] = 0.0f;
    lc.lightD[li][1] = 0.0f;
    lc.lightD[li][2] = 0.0f;
    lc.lightD[li][3] = 0.0f;
    ++li;
  }
  for (size_t i = 0; i < spotLights.size() && li < 16; ++i) {
    const RenderSpotLight& sl = spotLights[i];
    float angle = sl.angle;
    if (angle < 1.0f) {
      angle = 1.0f;
    }
    if (angle > 170.0f) {
      angle = 170.0f;
    }
    lc.lightA[li][0] = sl.pos[0];
    lc.lightA[li][1] = sl.pos[1];
    lc.lightA[li][2] = sl.pos[2];
    lc.lightA[li][3] = 1.0f;
    lc.lightB[li][0] = sl.dir[0];
    lc.lightB[li][1] = sl.dir[1];
    lc.lightB[li][2] = sl.dir[2];
    lc.lightB[li][3] = sl.radius;
    lc.lightC[li][0] = sl.color[0] * sl.intensity;
    lc.lightC[li][1] = sl.color[1] * sl.intensity;
    lc.lightC[li][2] = sl.color[2] * sl.intensity;
    lc.lightC[li][3] = cosf(angle * 0.5f * 0.01745329252f);
    float fw = sl.falloff;
    if (fw < 0.01f) {
      fw = 0.01f;
    }
    if (fw > 0.5f) {
      fw = 0.5f;
    }
    lc.lightD[li][0] = fw;
    lc.lightD[li][1] = 0.0f;
    lc.lightD[li][2] = 0.0f;
    lc.lightD[li][3] = 0.0f;
    ++li;
  }
  for (size_t i = 0; i < dirLights.size() && li < 16; ++i) {
    const RenderDirectionalLight& dl = dirLights[i];
    lc.lightA[li][0] = dl.dir[0];
    lc.lightA[li][1] = dl.dir[1];
    lc.lightA[li][2] = dl.dir[2];
    lc.lightA[li][3] = 2.0f;
    lc.lightB[li][0] = 0.0f;
    lc.lightB[li][1] = 0.0f;
    lc.lightB[li][2] = 0.0f;
    lc.lightB[li][3] = 0.0f;
    lc.lightC[li][0] = dl.color[0] * dl.intensity;
    lc.lightC[li][1] = dl.color[1] * dl.intensity;
    lc.lightC[li][2] = dl.color[2] * dl.intensity;
    lc.lightC[li][3] = 0.0f;
    lc.lightD[li][0] = 0.0f;
    lc.lightD[li][1] = 0.0f;
    lc.lightD[li][2] = 0.0f;
    lc.lightD[li][3] = 0.0f;
    ++li;
  }
  for (; li < 16; ++li) {
    lc.lightA[li][0] = 0.0f;
    lc.lightA[li][1] = 0.0f;
    lc.lightA[li][2] = 0.0f;
    lc.lightA[li][3] = 0.0f;
    lc.lightB[li][0] = 0.0f;
    lc.lightB[li][1] = 0.0f;
    lc.lightB[li][2] = 0.0f;
    lc.lightB[li][3] = 1.0f;
    lc.lightC[li][0] = 0.0f;
    lc.lightC[li][1] = 0.0f;
    lc.lightC[li][2] = 0.0f;
    lc.lightC[li][3] = 0.0f;
    lc.lightD[li][0] = 0.0f;
    lc.lightD[li][1] = 0.0f;
    lc.lightD[li][2] = 0.0f;
    lc.lightD[li][3] = 0.0f;
  }
  for (int c = 0; c < 4; ++c) {
    std::memcpy(lc.cascadeVP[c], shadowCascadeVP[c], sizeof(lc.cascadeVP[c]));
    lc.cascadeSplit[c] = shadowCascadeSplit[c];
    lc.cascadeNear[c] = shadowCascadeNear[c];
    lc.cascadeFar[c] = shadowCascadeFar[c];
    lc.cascadeUV[c] = shadowCascadeUV[c];
    lc.cascadeK[c] = shadowCascadeK[c];
    lc.cascadeSize[c] = shadowCascadeSize[c];
    lc.cascadeLight[c] = shadowCascadeLight[c];
  }
  lc.shadowInfo[0] = static_cast<float>(shadowCascadeActive);
  lc.shadowInfo[1] = shadowMapsValid ? 1.0f : 0.0f;
  lc.shadowInfo[2] = shadowDebug ? 1.0f : 0.0f;
  lc.shadowInfo[3] = 1.0f / static_cast<float>(ShadowAtlasSize);
  for (int s = 0; s < 4; ++s) {
    std::memcpy(lc.spotVP[s], shadowSpotVP[s], sizeof(lc.spotVP[s]));
    lc.spotMeta[s][0] = shadowSpotMeta[s][0];
    lc.spotMeta[s][1] = shadowSpotMeta[s][1];
    lc.spotMeta[s][2] = shadowSpotMeta[s][2];
    lc.spotMeta[s][3] = shadowSpotMeta[s][3];
  }
  lc.pointInfo[0] = shadowPointPos[0];
  lc.pointInfo[1] = shadowPointPos[1];
  lc.pointInfo[2] = shadowPointPos[2];
  lc.pointInfo[3] = shadowPointFar;
  lc.pointMeta[0] = static_cast<float>(shadowPointLight);
  lc.pointMeta[1] = shadowPointActive ? 1.0f : 0.0f;
  lc.pointMeta[2] = shadowPointNear;
  lc.pointMeta[3] = shadowPointSize;
  lc.gradeInfo[0] = exposure;
  lc.gradeInfo[1] = acesOn ? 1.0f : 0.0f;
  lc.gradeInfo[2] = 0.0f;
  lc.gradeInfo[3] = 0.0f;
  lc.camRight[0] = mainCam.camRight[0];
  lc.camRight[1] = mainCam.camRight[1];
  lc.camRight[2] = mainCam.camRight[2];
  lc.camRight[3] = std::tan(mainCam.fovY * 0.5f);
  lc.camUp[0] = mainCam.camUp[0];
  lc.camUp[1] = mainCam.camUp[1];
  lc.camUp[2] = mainCam.camUp[2];
  lc.camUp[3] = mainCam.aspect;
  float skyDir[3] = { -light.direction[0], -light.direction[1], -light.direction[2] };
  float skyCol[3] = { light.color[0], light.color[1], light.color[2] };
  float skyInt = light.intensity;
  if (!dirLights.empty()) {
    skyDir[0] = -dirLights[0].dir[0];
    skyDir[1] = -dirLights[0].dir[1];
    skyDir[2] = -dirLights[0].dir[2];
    skyCol[0] = dirLights[0].color[0];
    skyCol[1] = dirLights[0].color[1];
    skyCol[2] = dirLights[0].color[2];
    skyInt = dirLights[0].intensity;
  }
  float skyLen = std::sqrt(skyDir[0] * skyDir[0] + skyDir[1] * skyDir[1] + skyDir[2] * skyDir[2]);
  if (skyLen < 1e-6f) {
    skyDir[0] = 0.36f;
    skyDir[1] = 0.9f;
    skyDir[2] = 0.27f;
    skyLen = 1.0f;
  }
  lc.skySun[0] = skyDir[0] / skyLen;
  lc.skySun[1] = skyDir[1] / skyLen;
  lc.skySun[2] = skyDir[2] / skyLen;
  lc.skySun[3] = skyInt;
  lc.skyColor[0] = skyCol[0];
  lc.skyColor[1] = skyCol[1];
  lc.skyColor[2] = skyCol[2];
  lc.skyColor[3] = 0.0f;
  lc.ssaoInfo[0] = ssaoValid ? 1.0f : 0.0f;
  lc.ssaoInfo[1] = ssaoIntensity;
  lc.ssaoInfo[2] = 0.0f;
  lc.ssaoInfo[3] = 0.0f;
  std::memcpy(lc.ssaoVP, ssaoVP, sizeof(lc.ssaoVP));
  lc.fogInfo[0] = fogOn ? 1.0f : 0.0f;
  lc.fogInfo[1] = fogDensity;
  lc.fogInfo[2] = 0.0f;
  lc.fogInfo[3] = 0.0f;
  lc.fogColor[0] = fogColor[0];
  lc.fogColor[1] = fogColor[1];
  lc.fogColor[2] = fogColor[2];
  lc.fogColor[3] = 0.0f;
  rhi->UpdateConstantBuffer(lightCB, &lc, sizeof(lc));
  rhi->SetPixelConstantBuffer(0, lightCB);
  if (shadowMapsValid) {
    rhi->SetPixelTexture(4, shadowAtlas);
    rhi->SetPixelTexture(5, shadowCube);
    rhi->SetPixelSampler(1, shadowSampler);
  }
  if (ssaoValid) {
    rhi->SetPixelTexture(6, ssaoA);
  }
  rhi->DrawFullscreenTriangle();
  rhi->SetPixelTexture(0, 0);
  rhi->SetPixelTexture(1, 0);
  rhi->SetPixelTexture(2, 0);
  rhi->SetPixelTexture(3, 0);
  rhi->SetPixelTexture(4, 0);
  rhi->SetPixelTexture(5, 0);
  rhi->SetPixelTexture(6, 0);
  if (toTexture) {
    float bp[4] = { 0.8f, 0.0f, 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bp, sizeof(bp));
    rhi->SetPixelConstantBuffer(0, bloomCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetRenderTargets(1, &bloomA, 0);
    RHIViewport bvp;
    bvp.x = 0.0f;
    bvp.y = 0.0f;
    bvp.w = static_cast<float>(bloomW);
    bvp.h = static_cast<float>(bloomH);
    bvp.minD = 0.0f;
    bvp.maxD = 1.0f;
    rhi->SetViewport(bvp);
    rhi->SetPixelShader(bloomBrightPS);
    rhi->SetPixelTexture(0, gLight);
    rhi->DrawFullscreenTriangle();
    float bh[4] = { 1.0f / static_cast<float>(bloomW), 0.0f, 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bh, sizeof(bh));
    rhi->SetRenderTargets(1, &bloomB, 0);
    rhi->SetPixelShader(bloomBlurPS);
    rhi->SetPixelTexture(0, bloomA);
    rhi->DrawFullscreenTriangle();
    float bv[4] = { 0.0f, 1.0f / static_cast<float>(bloomH), 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bv, sizeof(bv));
    rhi->SetRenderTargets(1, &bloomA, 0);
    rhi->SetPixelTexture(0, bloomB);
    rhi->DrawFullscreenTriangle();
    float ba[4] = { bloomStrength, 0.0f, 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &ba, sizeof(ba));
    rhi->SetRenderTargets(1, &gViewport, 0);
    RHIViewport fvp;
    fvp.x = viewX;
    fvp.y = viewY;
    fvp.w = static_cast<float>(w);
    fvp.h = static_cast<float>(h);
    fvp.minD = 0.0f;
    fvp.maxD = 1.0f;
    rhi->SetViewport(fvp);
    rhi->SetPixelShader(bloomAddPS);
    rhi->SetPixelTexture(0, gLight);
    rhi->SetPixelTexture(1, bloomA);
    rhi->DrawFullscreenTriangle();
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    bool fsrActive = renderScale < 0.999f && fsrA != 0 && fsrOut != 0;
    if (fsrActive) {
      AU1 con0[4];
      AU1 con1[4];
      AU1 con2[4];
      AU1 con3[4];
      FsrEasuCon(con0, con1, con2, con3, (AF1)w, (AF1)h, (AF1)w, (AF1)h, (AF1)outW, (AF1)outH);
      uint32_t easu[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      std::memcpy(&easu[0], con0, sizeof(uint32_t) * 4);
      std::memcpy(&easu[4], con1, sizeof(uint32_t) * 4);
      std::memcpy(&easu[8], con2, sizeof(uint32_t) * 4);
      std::memcpy(&easu[12], con3, sizeof(uint32_t) * 4);
      rhi->UpdateConstantBuffer(fsrCB, &easu, sizeof(easu));
      rhi->SetPixelConstantBuffer(0, fsrCB);
      rhi->SetPixelSampler(0, sampler);
      rhi->SetRenderTargets(1, &fsrA, 0);
      RHIViewport fsvp;
      fsvp.x = 0.0f;
      fsvp.y = 0.0f;
      fsvp.w = static_cast<float>(outW);
      fsvp.h = static_cast<float>(outH);
      fsvp.minD = 0.0f;
      fsvp.maxD = 1.0f;
      rhi->SetViewport(fsvp);
      rhi->SetPixelShader(fsrEasuPS);
      rhi->SetPixelTexture(0, gViewport);
      rhi->DrawFullscreenTriangle();
      AU1 rcon[4];
      FsrRcasCon(rcon, (AF1)((1.0f - fsrSharpness) * 2.0f));
      uint32_t rcas[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      std::memcpy(&rcas[0], rcon, sizeof(uint32_t) * 4);
      rhi->UpdateConstantBuffer(fsrCB, &rcas, sizeof(rcas));
      rhi->SetRenderTargets(1, &fsrOut, 0);
      rhi->SetPixelShader(fsrRcasPS);
      rhi->SetPixelTexture(0, fsrA);
      rhi->DrawFullscreenTriangle();
      rhi->SetPixelTexture(0, 0);
    }
    rhi->BindBackbuffer();
  }
}
bool DeferredRenderer::IsReady() const {
  return ready;
}
bool DeferredRenderer::CreateTargets() {
  if (rhi == nullptr) {
    return false;
  }
  gAlbedo = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gNormalRough = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gMetallic = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gPosition = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gDepth = rhi->CreateRenderTarget(w, h, RHIFormat::D24S8);
  gViewport = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gLight = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  bloomW = w / 2;
  if (bloomW < 1) {
    bloomW = 1;
  }
  bloomH = h / 2;
  if (bloomH < 1) {
    bloomH = 1;
  }
  bloomA = rhi->CreateRenderTarget(bloomW, bloomH, RHIFormat::RGBA8_UNORM);
  bloomB = rhi->CreateRenderTarget(bloomW, bloomH, RHIFormat::RGBA8_UNORM);
  int fw = outW > 0 ? outW : w;
  int fh = outH > 0 ? outH : h;
  fsrA = rhi->CreateRenderTarget(fw, fh, RHIFormat::RGBA8_UNORM);
  fsrOut = rhi->CreateRenderTarget(fw, fh, RHIFormat::RGBA8_UNORM);
  ssaoW = w / 2;
  if (ssaoW < 1) {
    ssaoW = 1;
  }
  ssaoH = h / 2;
  if (ssaoH < 1) {
    ssaoH = 1;
  }
  ssaoA = rhi->CreateRenderTarget(ssaoW, ssaoH, RHIFormat::RGBA8_UNORM);
  ssaoB = rhi->CreateRenderTarget(ssaoW, ssaoH, RHIFormat::RGBA8_UNORM);
  return gAlbedo != 0 && gNormalRough != 0 && gMetallic != 0 && gPosition != 0 && gDepth != 0 && gViewport != 0 && gLight != 0 && bloomA != 0 && bloomB != 0 && fsrA != 0 && fsrOut != 0 && ssaoA != 0 && ssaoB != 0;
}
void DeferredRenderer::DestroyTargets() {
  if (rhi == nullptr) {
    return;
  }
  if (gAlbedo != 0) {
    rhi->DestroyRenderTarget(gAlbedo);
    gAlbedo = 0;
  }
  if (gNormalRough != 0) {
    rhi->DestroyRenderTarget(gNormalRough);
    gNormalRough = 0;
  }
  if (gMetallic != 0) {
    rhi->DestroyRenderTarget(gMetallic);
    gMetallic = 0;
  }
  if (gPosition != 0) {
    rhi->DestroyRenderTarget(gPosition);
    gPosition = 0;
  }
  if (gDepth != 0) {
    rhi->DestroyRenderTarget(gDepth);
    gDepth = 0;
  }
  if (gViewport != 0) {
    rhi->DestroyRenderTarget(gViewport);
    gViewport = 0;
  }
  if (gLight != 0) {
    rhi->DestroyRenderTarget(gLight);
    gLight = 0;
  }
  if (bloomA != 0) {
    rhi->DestroyRenderTarget(bloomA);
    bloomA = 0;
  }
  if (bloomB != 0) {
    rhi->DestroyRenderTarget(bloomB);
    bloomB = 0;
  }
  if (fsrA != 0) {
    rhi->DestroyRenderTarget(fsrA);
    fsrA = 0;
  }
  if (fsrOut != 0) {
    rhi->DestroyRenderTarget(fsrOut);
    fsrOut = 0;
  }
  if (ssaoA != 0) {
    rhi->DestroyRenderTarget(ssaoA);
    ssaoA = 0;
  }
  if (ssaoB != 0) {
    rhi->DestroyRenderTarget(ssaoB);
    ssaoB = 0;
  }
}
}
