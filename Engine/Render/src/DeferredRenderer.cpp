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
  float probeBoxMin[4][4];
  float probeBoxMax[4][4];
};
struct MatConstants {
  float albedo[4];
  float params[4];
  float extra[4];
  float extA[4];
  float extB[4];
  float extC[4];
  float extD[4];
  float extE[4];
  float extF[4];
  float extG[4];
};
struct MatExtConstants {
  float extA[4];
  float extB[4];
  float extC[4];
  float extD[4];
  float extE[4];
  float extF[4];
  float extG[4];
  float extH[4];
  float xfA[5][4];
  float xfB[5][4];
};
void FillMatConstants(const DeferredMaterial& use, float lodFade, MatConstants& mc, MatExtConstants& mec, unsigned char* tablePx, uint32_t& rows, uint32_t& outRow) {
  mc.albedo[0] = use.albedo[0];
  mc.albedo[1] = use.albedo[1];
  mc.albedo[2] = use.albedo[2];
  mc.albedo[3] = use.alpha;
  mc.params[0] = use.roughness;
  mc.params[1] = use.metallic;
  mc.params[2] = static_cast<float>(use.texFlags);
  mc.params[3] = lodFade < 0.0f ? 0.0f : (lodFade > 1.0f ? 1.0f : lodFade);
  mc.extra[0] = use.lightmapIntensity;
  mc.extra[1] = 0.0f;
  mc.extra[2] = 0.0f;
  mc.extra[3] = 0.0f;
  mc.extA[0] = use.emissiveStrength;
  mc.extA[1] = use.ior;
  mc.extA[2] = use.specularStrength;
  mc.extA[3] = use.alphaCutoff;
  mc.extB[0] = static_cast<float>(use.alphaMode);
  mc.extB[1] = use.doubleSided ? 1.0f : 0.0f;
  mc.extB[2] = use.clearcoat;
  mc.extB[3] = use.clearcoatRoughness;
  mc.extC[0] = use.sheenRoughness;
  mc.extC[1] = use.iridescence;
  mc.extC[2] = use.iridescenceThicknessMin;
  mc.extC[3] = use.iridescenceThicknessMax;
  mc.extD[0] = use.anisotropy;
  mc.extD[1] = use.anisotropyRotation;
  mc.extD[2] = use.dispersion;
  mc.extD[3] = use.attenuationDistance;
  mc.extE[0] = use.emissive[0];
  mc.extE[1] = use.emissive[1];
  mc.extE[2] = use.emissive[2];
  mc.extF[0] = use.specularColor[0];
  mc.extF[1] = use.specularColor[1];
  mc.extF[2] = use.specularColor[2];
  mc.extF[3] = use.transmission;
  mc.extG[0] = use.sheenColor[0];
  mc.extG[1] = use.sheenColor[1];
  mc.extG[2] = use.sheenColor[2];
  mc.extG[3] = 0.0f;
  uint32_t row = 0;
  if (rows < 256) {
    row = rows++;
    unsigned char* px = tablePx + static_cast<size_t>(row) * 4;
    px[0] = use.unlit ? 255 : 0;
    float iri = (use.iridescenceIor - 1.0f) * 0.5f;
    px[1] = static_cast<unsigned char>((iri < 0.0f ? 0.0f : (iri > 1.0f ? 1.0f : iri)) * 255.0f + 0.5f);
    float disp = use.dispersion >= 0.0f ? use.dispersion : 0.0f;
    px[2] = static_cast<unsigned char>((disp > 1.0f ? 1.0f : disp) * 255.0f + 0.5f);
    float ior = (use.ior - 1.0f) / 3.0f;
    px[3] = static_cast<unsigned char>((ior < 0.0f ? 0.0f : (ior > 1.0f ? 1.0f : ior)) * 255.0f + 0.5f);
  }
  outRow = row;
  mc.extE[3] = static_cast<float>(row);
  mec.extA[0] = mc.extA[0];
  mec.extA[1] = mc.extA[1];
  mec.extA[2] = mc.extA[2];
  mec.extA[3] = mc.extA[3];
  mec.extB[0] = mc.extB[0];
  mec.extB[1] = mc.extB[1];
  mec.extB[2] = mc.extB[2];
  mec.extB[3] = mc.extB[3];
  mec.extC[0] = mc.extC[0];
  mec.extC[1] = mc.extC[1];
  mec.extC[2] = mc.extC[2];
  mec.extC[3] = mc.extC[3];
  mec.extD[0] = mc.extD[0];
  mec.extD[1] = mc.extD[1];
  mec.extD[2] = mc.extD[2];
  mec.extD[3] = mc.extD[3];
  mec.extE[0] = mc.extE[0];
  mec.extE[1] = mc.extE[1];
  mec.extE[2] = mc.extE[2];
  mec.extE[3] = mc.extE[3];
  mec.extF[0] = mc.extF[0];
  mec.extF[1] = mc.extF[1];
  mec.extF[2] = mc.extF[2];
  mec.extF[3] = mc.extF[3];
  mec.extG[0] = mc.extG[0];
  mec.extG[1] = mc.extG[1];
  mec.extG[2] = mc.extG[2];
  mec.extG[3] = mc.extG[3];
  mec.extH[0] = use.attenuationColor[0];
  mec.extH[1] = use.attenuationColor[1];
  mec.extH[2] = use.attenuationColor[2];
  mec.extH[3] = use.thickness;
  for (int s = 0; s < 5; ++s) {
    mec.xfA[s][0] = use.xforms[s].offsetU;
    mec.xfA[s][1] = use.xforms[s].offsetV;
    mec.xfA[s][2] = use.xforms[s].scaleU;
    mec.xfA[s][3] = use.xforms[s].scaleV;
    mec.xfB[s][0] = use.xforms[s].rotation;
    mec.xfB[s][1] = static_cast<float>(use.xforms[s].uvSet);
    mec.xfB[s][2] = 0.0f;
    mec.xfB[s][3] = 0.0f;
  }
}
struct SsrConstants {
  float viewProj[16];
  float camPos[4];
  float params[4];
  float screen[4];
};
struct PostConstants {
  float prevVP[16];
  float camPos[4];
  float focus[4];
  float film[4];
  float toggles[4];
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
  , gLightmap(0)
  , gEmissive(0)
  , gF0(0)
  , gExtA(0)
  , gDepth(0)
  , gViewport(0)
  , vb(0)
  , ib(0)
  , geoCB(0)
  , lightCB(0)
  , matCB(0)
  , matExtCB(0)
  , matTable(0)
  , matTableRows(0)
  , neutralBlack(0)
  , neutralAniso(0)
  , frustumValid(false)
  , meshHasBounds(false)
  , culledDraws(0)
  , geoVS(0)
  , geoPS(0)
  , forwardPS(0)
  , copyPS(0)
  , forwardCB(0)
  , lightVS(0)
  , lightPS(0)
  , depthVS(0)
  , decalVS(0)
  , decalPS(0)
  , decalCB(0)
  , decalVB(0)
  , decalIB(0)
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
  , ssrPS(0)
  , ssrCB(0)
  , ssrA(0)
  , ssrW(1)
  , ssrH(1)
  , ssrOn(true)
  , ssrValid(false)
  , contactPS(0)
  , contactA(0)
  , contactOn(true)
  , filmPS(0)
  , filmCB(0)
  , filmA(0)
  , ssgiPS(0)
  , ssgiA(0)
  , ssgiOn(true)
  , ssgiBlurPS(0)
  , ssgiB(0)
  , fxaaPS(0)
  , fxaaCB(0)
  , fxaaA(0)
  , fxaaOn(true)
  , fxaaRan(false)
  , gradePS(0)
  , gradeCB(0)
  , gradeA(0)
  , toneMode(0)
  , envCube(0)
  , captureTarget(0)
  , captureMode(false)
  , envIntensity(1.0f)
  , captureCube(0)
  , probeCount(0)
  , dofOn(false)
  , focusDist(10.0f)
  , focusRange(20.0f)
  , motionOn(false)
  , motionScale(1.0f)
  , filmCa(0.0f)
  , filmVig(0.0f)
  , filmGrain(0.0f)
  , filmActive(false)
  , frameIndex(0)
  , layout(0)
  , sampler(0)
  , geoSampler(0)
  , neutralAlbedo(0)
  , neutralNormal(0)
  , neutralOrm(0)
  , indexCount(0)
  , viewX(0.0f)
  , viewY(0.0f)
  , begun(false)
  , ready(false)
  , gpuShadowTs(0)
  , gpuLightTs(0)
  , gpuPostTs(0)
  , dynResOn(false)
  , dynTargetMs(16.6f)
  , dynMin(0.5f)
  , dynMax(1.0f)
  , dynScale(1.0f)
  , dynFrames(0)
  , dynAccum(0.0)
  , occOn(false)
  , occFrame(0)
  , occActive(0)
  , occBudget(0)
  , geoVSInstanced(0)
  , instLayout(0)
  , instVB(0)
  , instCap(0)
  , instHasLast(false)
  , instLastKey(0)
  , instLastHash(0)
  , batchOk(true)
  , shadowCacheOn(true)
  , shadowAutoRes(false)
  , shadowStreak(0)
  , shadowLastWant(0)
  , shadowAtlasSize(4096)
  , shadowTileSize(2048)
  , shadowCubeSize(512)
  , shadowResLevel(0)
  , hasCasterHash(false)
  , lastCasterHash(0)
  , shadowAutoFrames(0)
  , shadowAutoAccum(0.0) {
  material.albedo[0] = 0.8f;
  material.albedo[1] = 0.2f;
  fogColor[0] = 0.6f;
  fogColor[1] = 0.7f;
  fogColor[2] = 0.8f;
  material.albedo[2] = 0.15f;
  material.roughness = 0.5f;
  material.metallic = 0.0f;
  material.albedoTex = 0;
  material.normalTex = 0;
  material.ormTex = 0;
  material.heightTex = 0;
  material.lightmapTex = 0;
  material.lightmapIntensity = 1.0f;
  material.texFlags = 0;
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
  for (int i = 0; i < 4; ++i) {
    probeCubes[i] = 0;
    probeBoxMin[i][0] = 0.0f;
    probeBoxMin[i][1] = 0.0f;
    probeBoxMin[i][2] = 0.0f;
    probeBoxMin[i][3] = 0.0f;
    probeBoxMax[i][0] = 0.0f;
    probeBoxMax[i][1] = 0.0f;
    probeBoxMax[i][2] = 0.0f;
    probeBoxMax[i][3] = 0.0f;
  }
  for (int i = 0; i < 16; ++i) {
    prevView[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    prevProj[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    savedLastView[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    savedLastProj[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    savedPrevView[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    savedPrevProj[i] = (i % 5 == 0) ? 1.0f : 0.0f;
  }
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
  std::string decalVSPath = dir + "/DecalVS.hlsl";
  std::string decalPSPath = dir + "/DecalPS.hlsl";
  std::string bloomBrightPath = dir + "/BloomBrightPS.hlsl";
  std::string bloomBlurPath = dir + "/BloomBlurPS.hlsl";
  std::string bloomAddPath = dir + "/BloomAddPS.hlsl";
  std::string fsrEasuPath = dir + "/FsrEasuPS.hlsl";
  std::string fsrRcasPath = dir + "/FsrRcasPS.hlsl";
  std::string ssaoPath = dir + "/SsaoPS.hlsl";
  std::string ssaoBlurPath = dir + "/SsaoBlurPS.hlsl";
  std::string ssrPath = dir + "/SsrPS.hlsl";
  std::string contactPath = dir + "/ContactPS.hlsl";
  std::string filmPath = dir + "/FilmPS.hlsl";
  std::string ssgiPath = dir + "/SsgiPS.hlsl";
  std::string ssgiBlurPath = dir + "/SsgiBlurPS.hlsl";
  std::string fxaaPath = dir + "/FxaaPS.hlsl";
  std::string forwardPath = dir + "/ForwardPS.hlsl";
  std::string copyPath = dir + "/CopyPS.hlsl";
  geoVS = rhi->CreateVertexShaderFromFile(geoVSPath.c_str(), "main");
  geoPS = rhi->CreatePixelShaderFromFile(geoPSPath.c_str(), "main");
  lightVS = rhi->CreateVertexShaderFromFile(lightVSPath.c_str(), "main");
  lightPS = rhi->CreatePixelShaderFromFile(lightPSPath.c_str(), "main");
  depthVS = rhi->CreateVertexShaderFromFile(depthVSPath.c_str(), "main");
  decalVS = rhi->CreateVertexShaderFromFile(decalVSPath.c_str(), "main");
  decalPS = rhi->CreatePixelShaderFromFile(decalPSPath.c_str(), "main");
  bloomBrightPS = rhi->CreatePixelShaderFromFile(bloomBrightPath.c_str(), "main");
  bloomBlurPS = rhi->CreatePixelShaderFromFile(bloomBlurPath.c_str(), "main");
  bloomAddPS = rhi->CreatePixelShaderFromFile(bloomAddPath.c_str(), "main");
  if (geoVS == 0 || geoPS == 0 || lightVS == 0 || lightPS == 0 || depthVS == 0) {
    return false;
  }
  if (decalVS == 0 || decalPS == 0) {
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
  ssrPS = rhi->CreatePixelShaderFromFile(ssrPath.c_str(), "main");
  if (ssrPS == 0) {
    return false;
  }
  contactPS = rhi->CreatePixelShaderFromFile(contactPath.c_str(), "main");
  if (contactPS == 0) {
    return false;
  }
  filmPS = rhi->CreatePixelShaderFromFile(filmPath.c_str(), "main");
  if (filmPS == 0) {
    return false;
  }
  ssgiPS = rhi->CreatePixelShaderFromFile(ssgiPath.c_str(), "main");
  if (ssgiPS == 0) {
    return false;
  }
  ssgiBlurPS = rhi->CreatePixelShaderFromFile(ssgiBlurPath.c_str(), "main");
  if (ssgiBlurPS == 0) {
    return false;
  }
  fxaaPS = rhi->CreatePixelShaderFromFile(fxaaPath.c_str(), "main");
  if (fxaaPS == 0) {
    return false;
  }
  std::string gradePath = dir + "/GradePS.hlsl";
  gradePS = rhi->CreatePixelShaderFromFile(gradePath.c_str(), "main");
  if (gradePS == 0) {
    return false;
  }
  float gradeInit[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
  gradeCB = rhi->CreateConstantBuffer(sizeof(gradeInit), &gradeInit);
  if (gradeCB == 0) {
    return false;
  }
  forwardPS = rhi->CreatePixelShaderFromFile(forwardPath.c_str(), "main");
  if (forwardPS == 0) {
    return false;
  }
  copyPS = rhi->CreatePixelShaderFromFile(copyPath.c_str(), "main");
  if (copyPS == 0) {
    return false;
  }
  layout = rhi->CreateInputLayoutPNUT(geoVS);
  if (layout == 0) {
    return false;
  }
  geoVSInstanced = rhi->CreateVertexShaderFromFile(geoVSPath.c_str(), "mainInst");
  if (geoVSInstanced != 0) {
    instLayout = rhi->CreateInputLayoutPNUTInstanced(geoVSInstanced);
  }
  if (geoVSInstanced == 0 || instLayout == 0) {
    batchOk = false;
  }
  GeoConstants gc;
  std::memset(&gc, 0, sizeof(gc));
  LightConstants lc;
  std::memset(&lc, 0, sizeof(lc));
  MatConstants mc;
  std::memset(&mc, 0, sizeof(mc));
  MatExtConstants mec;
  std::memset(&mec, 0, sizeof(mec));
  float bloomInit[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  uint32_t fsrInit[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  float ssaoInit[24] = { 0.0f };
  SsrConstants sc;
  std::memset(&sc, 0, sizeof(sc));
  geoCB = rhi->CreateConstantBuffer(sizeof(GeoConstants), &gc);
  lightCB = rhi->CreateConstantBuffer(sizeof(LightConstants), &lc);
  matCB = rhi->CreateConstantBuffer(sizeof(MatConstants), &mc);
  matExtCB = rhi->CreateConstantBuffer(sizeof(MatExtConstants), &mec);
  ssrCB = rhi->CreateConstantBuffer(sizeof(SsrConstants), &sc);
  if (ssrCB == 0) {
    return false;
  }
  PostConstants pc;
  std::memset(&pc, 0, sizeof(pc));
  filmCB = rhi->CreateConstantBuffer(sizeof(PostConstants), &pc);
  if (filmCB == 0) {
    return false;
  }
  float fxaaInit[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  fxaaCB = rhi->CreateConstantBuffer(sizeof(fxaaInit), &fxaaInit);
  if (fxaaCB == 0) {
    return false;
  }
  float fwdInit[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
  forwardCB = rhi->CreateConstantBuffer(sizeof(fwdInit), &fwdInit);
  if (forwardCB == 0) {
    return false;
  }
  bloomCB = rhi->CreateConstantBuffer(sizeof(bloomInit), &bloomInit);
  fsrCB = rhi->CreateConstantBuffer(sizeof(fsrInit), &fsrInit);
  ssaoCB = rhi->CreateConstantBuffer(sizeof(ssaoInit), &ssaoInit);
  if (geoCB == 0 || lightCB == 0 || matCB == 0 || matExtCB == 0) {
    return false;
  }
  if (bloomCB == 0 || fsrCB == 0 || ssaoCB == 0) {
    return false;
  }
  sampler = rhi->CreateSamplerLinear();
  if (sampler == 0) {
    return false;
  }
  geoSampler = rhi->CreateSamplerWrap();
  if (geoSampler == 0) {
    return false;
  }
  unsigned char whitePx[4] = { 255, 255, 255, 255 };
  unsigned char nrmPx[4] = { 128, 128, 255, 255 };
  unsigned char ormPx[4] = { 255, 128, 0, 255 };
  unsigned char blackPx[4] = { 0, 0, 0, 255 };
  unsigned char anisoPx[4] = { 255, 128, 255, 255 };
  neutralAlbedo = rhi->CreateTexture2D(1, 1, 1, RHITextureFormat::RGBA8_UNORM);
  neutralNormal = rhi->CreateTexture2D(1, 1, 1, RHITextureFormat::RGBA8_UNORM);
  neutralOrm = rhi->CreateTexture2D(1, 1, 1, RHITextureFormat::RGBA8_UNORM);
  neutralBlack = rhi->CreateTexture2D(1, 1, 1, RHITextureFormat::RGBA8_UNORM);
  neutralAniso = rhi->CreateTexture2D(1, 1, 1, RHITextureFormat::RGBA8_UNORM);
  if (neutralAlbedo == 0 || neutralNormal == 0 || neutralOrm == 0 || neutralBlack == 0 || neutralAniso == 0) {
    return false;
  }
  if (!rhi->UpdateTextureMip(neutralAlbedo, 0, 1, 1, 4, whitePx, 4) || !rhi->UpdateTextureMip(neutralNormal, 0, 1, 1, 4, nrmPx, 4) || !rhi->UpdateTextureMip(neutralOrm, 0, 1, 1, 4, ormPx, 4) || !rhi->UpdateTextureMip(neutralBlack, 0, 1, 1, 4, blackPx, 4) || !rhi->UpdateTextureMip(neutralAniso, 0, 1, 1, 4, anisoPx, 4)) {
    return false;
  }
  matTable = rhi->CreateTexture2D(256, 1, 1, RHITextureFormat::RGBA8_UNORM);
  if (matTable == 0) {
    return false;
  }
  std::memset(matTablePx, 0, sizeof(matTablePx));
  matTablePx[0] = 0;
  matTablePx[1] = 38;
  matTablePx[2] = 0;
  matTablePx[3] = 43;
  matTableRows = 1;
  if (!rhi->UpdateTextureMip(matTable, 0, 256, 1, 256 * 4, matTablePx, sizeof(matTablePx))) {
    return false;
  }
  float decalBox[8 * 14] = {
    -0.5f, -0.5f, -0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0.5f, -0.5f, -0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0.5f, 0.5f, -0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    -0.5f, 0.5f, -0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    -0.5f, -0.5f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0.5f, -0.5f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0.5f, 0.5f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    -0.5f, 0.5f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0
  };
  uint32_t decalIdx[36] = {
    0, 1, 2, 0, 2, 3,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    1, 5, 6, 1, 6, 2,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0
  };
  decalVB = rhi->CreateBuffer(sizeof(decalBox), 56, false, decalBox);
  decalIB = rhi->CreateBuffer(sizeof(decalIdx), 4, true, decalIdx);
  if (decalVB == 0 || decalIB == 0) {
    return false;
  }
  float decalInit[16] = { 0.0f };
  decalCB = rhi->CreateConstantBuffer(sizeof(decalInit), &decalInit);
  if (decalCB == 0) {
    return false;
  }
  shadowAtlas = rhi->CreateRenderTarget(shadowAtlasSize, shadowAtlasSize, RHIFormat::R32_DEPTH);
  shadowCube = rhi->CreateShadowCube(shadowCubeSize);
  shadowSampler = rhi->CreateSamplerShadow();
  shadowsAvailable = shadowAtlas != 0 && shadowCube != 0 && shadowSampler != 0;
  if (!CreateTargets()) {
    return false;
  }
  gpuShadowTs = rhi->CreateTimestamp();
  gpuLightTs = rhi->CreateTimestamp();
  gpuPostTs = rhi->CreateTimestamp();
  ready = true;
  return true;
}
void DeferredRenderer::Shutdown() {
  if (rhi == nullptr) {
    return;
  }
  DestroyTargets();
  for (auto& kv : occMap) {
    if (kv.second.q != 0) {
      rhi->DestroyOcclusionQuery(kv.second.q);
    }
  }
  occMap.clear();
  occActive = 0;
  for (auto& kv : occGroups) {
    if (kv.second.q != 0) {
      rhi->DestroyOcclusionQuery(kv.second.q);
    }
  }
  occGroups.clear();
  if (instVB != 0) {
    rhi->DestroyBuffer(instVB);
    instVB = 0;
  }
  instCap = 0;
  instHasLast = false;
  if (gpuShadowTs != 0) {
    rhi->DestroyTimestamp(gpuShadowTs);
    gpuShadowTs = 0;
  }
  if (gpuLightTs != 0) {
    rhi->DestroyTimestamp(gpuLightTs);
    gpuLightTs = 0;
  }
  if (gpuPostTs != 0) {
    rhi->DestroyTimestamp(gpuPostTs);
    gpuPostTs = 0;
  }
  if (envCube != 0) {
    rhi->DestroyTexture(envCube);
    envCube = 0;
  }
  captureTarget = 0;
  captureMode = false;
  if (decalVB != 0) {
    rhi->DestroyBuffer(decalVB);
    decalVB = 0;
  }
  if (decalIB != 0) {
    rhi->DestroyBuffer(decalIB);
    decalIB = 0;
  }
  if (decalCB != 0) {
    rhi->DestroyConstantBuffer(decalCB);
    decalCB = 0;
  }
  if (neutralAlbedo != 0) {
    rhi->DestroyTexture(neutralAlbedo);
    neutralAlbedo = 0;
  }
  if (neutralNormal != 0) {
    rhi->DestroyTexture(neutralNormal);
    neutralNormal = 0;
  }
  if (neutralOrm != 0) {
    rhi->DestroyTexture(neutralOrm);
    neutralOrm = 0;
  }
  if (neutralBlack != 0) {
    rhi->DestroyTexture(neutralBlack);
    neutralBlack = 0;
  }
  if (neutralAniso != 0) {
    rhi->DestroyTexture(neutralAniso);
    neutralAniso = 0;
  }
  if (matTable != 0) {
    rhi->DestroyTexture(matTable);
    matTable = 0;
  }
  geoSampler = 0;
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
  if (matExtCB != 0) {
    rhi->DestroyConstantBuffer(matExtCB);
    matExtCB = 0;
  }
  if (forwardCB != 0) {
    rhi->DestroyConstantBuffer(forwardCB);
    forwardCB = 0;
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
  if (ssrCB != 0) {
    rhi->DestroyConstantBuffer(ssrCB);
    ssrCB = 0;
  }
  if (filmCB != 0) {
    rhi->DestroyConstantBuffer(filmCB);
    filmCB = 0;
  }
  if (fxaaCB != 0) {
    rhi->DestroyConstantBuffer(fxaaCB);
    fxaaCB = 0;
  }
  if (gradeCB != 0) {
    rhi->DestroyConstantBuffer(gradeCB);
    gradeCB = 0;
  }
  rhi = nullptr;
  ready = false;
}
bool DeferredRenderer::SetMesh(const float* positions, const float* normals, const float* uvs, size_t vertexCount, const uint32_t* indices, size_t idxCount, const float* tangents, const float* uv2) {
  if (rhi == nullptr || positions == nullptr || normals == nullptr || indices == nullptr) {
    return false;
  }
  if (vertexCount == 0 || idxCount == 0) {
    return false;
  }
  meshMin[0] = positions[0];
  meshMin[1] = positions[1];
  meshMin[2] = positions[2];
  meshMax[0] = positions[0];
  meshMax[1] = positions[1];
  meshMax[2] = positions[2];
  for (size_t i = 1; i < vertexCount; ++i) {
    if (positions[i * 3 + 0] < meshMin[0]) {
      meshMin[0] = positions[i * 3 + 0];
    }
    if (positions[i * 3 + 1] < meshMin[1]) {
      meshMin[1] = positions[i * 3 + 1];
    }
    if (positions[i * 3 + 2] < meshMin[2]) {
      meshMin[2] = positions[i * 3 + 2];
    }
    if (positions[i * 3 + 0] > meshMax[0]) {
      meshMax[0] = positions[i * 3 + 0];
    }
    if (positions[i * 3 + 1] > meshMax[1]) {
      meshMax[1] = positions[i * 3 + 1];
    }
    if (positions[i * 3 + 2] > meshMax[2]) {
      meshMax[2] = positions[i * 3 + 2];
    }
  }
  meshHasBounds = true;
  std::vector<float> interleaved;
  interleaved.reserve(vertexCount * 14);
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
    float llu = 0.0f;
    float llv = 0.0f;
    if (uv2 != nullptr) {
      llu = uv2[i * 2 + 0];
      llv = uv2[i * 2 + 1];
    }
    interleaved.push_back(llu);
    interleaved.push_back(llv);
  }
  uint64_t vbSize = static_cast<uint64_t>(interleaved.size() * sizeof(float));
  uint64_t ibSize = static_cast<uint64_t>(idxCount * sizeof(uint32_t));
  RHIBuffer nvb = rhi->CreateBuffer(vbSize, 56, false, interleaved.data());
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
  toneMode = aces ? 0 : 3;
}
void DeferredRenderer::SetToneMapping(int mode) {
  if (mode < 0 || mode > 3) {
    return;
  }
  toneMode = mode;
  acesOn = mode == 0;
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
void DeferredRenderer::SetSsr(bool on) {
  ssrOn = on;
}
void DeferredRenderer::SetContact(bool on) {
  contactOn = on;
}
void DeferredRenderer::SetDof(bool on, float dist, float range) {
  dofOn = on;
  focusDist = dist;
  focusRange = range;
  if (focusRange < 0.5f) {
    focusRange = 0.5f;
  }
}
void DeferredRenderer::SetMotionBlur(bool on, float scale) {
  motionOn = on;
  motionScale = scale;
  if (motionScale < 0.0f) {
    motionScale = 0.0f;
  }
  if (motionScale > 4.0f) {
    motionScale = 4.0f;
  }
}
void DeferredRenderer::SetFilm(float ca, float vignette, float grain) {
  filmCa = ca < 0.0f ? 0.0f : (ca > 2.0f ? 2.0f : ca);
  filmVig = vignette < 0.0f ? 0.0f : (vignette > 1.0f ? 1.0f : vignette);
  filmGrain = grain < 0.0f ? 0.0f : (grain > 0.25f ? 0.25f : grain);
}
void DeferredRenderer::SetSsgi(bool on) {
  ssgiOn = on;
}
void DeferredRenderer::SetFxaa(bool on) {
  fxaaOn = on;
}
void DeferredRenderer::SetEnvIntensity(float intensity) {
  envIntensity = intensity;
  if (envIntensity < 0.0f) {
    envIntensity = 0.0f;
  }
  if (envIntensity > 4.0f) {
    envIntensity = 4.0f;
  }
}
void DeferredRenderer::SetEnvProbes(int count, const RHITexture* cubes, const float (*boxMin)[4], const float (*boxMax)[4]) {
  probeCount = 0;
  if (count < 0) {
    count = 0;
  }
  if (count > 4) {
    count = 4;
  }
  for (int i = 0; i < 4; ++i) {
    probeCubes[i] = 0;
  }
  if (cubes == nullptr || boxMin == nullptr || boxMax == nullptr) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    if (cubes[i] == 0) {
      continue;
    }
    probeCubes[probeCount] = cubes[i];
    probeBoxMin[probeCount][0] = boxMin[i][0];
    probeBoxMin[probeCount][1] = boxMin[i][1];
    probeBoxMin[probeCount][2] = boxMin[i][2];
    probeBoxMin[probeCount][3] = boxMin[i][3];
    probeBoxMax[probeCount][0] = boxMax[i][0];
    probeBoxMax[probeCount][1] = boxMax[i][1];
    probeBoxMax[probeCount][2] = boxMax[i][2];
    probeBoxMax[probeCount][3] = boxMax[i][3];
    ++probeCount;
  }
}
bool DeferredRenderer::BeginEnvCapture(int size) {
  if (rhi == nullptr || size <= 0) {
    return false;
  }
  if (envCube != 0) {
    rhi->DestroyTexture(envCube);
    envCube = 0;
  }
  captureTarget = 0;
  captureMode = false;
  std::memcpy(savedLastView, lastView, sizeof(savedLastView));
  std::memcpy(savedLastProj, lastProj, sizeof(savedLastProj));
  std::memcpy(savedPrevView, prevView, sizeof(savedPrevView));
  std::memcpy(savedPrevProj, prevProj, sizeof(savedPrevProj));
  envCube = rhi->CreateEnvCube(size);
  captureCube = envCube;
  return envCube != 0;
}
bool DeferredRenderer::BeginProbeCapture(RHITexture cube) {
  if (rhi == nullptr || cube == 0) {
    return false;
  }
  captureTarget = 0;
  captureMode = false;
  captureCube = cube;
  return true;
}
void DeferredRenderer::GenerateEnvMips() {
  if (rhi == nullptr || captureCube == 0) {
    return;
  }
  rhi->GenerateCubeMips(captureCube);
  std::memcpy(lastView, savedLastView, sizeof(lastView));
  std::memcpy(lastProj, savedLastProj, sizeof(lastProj));
  std::memcpy(prevView, savedPrevView, sizeof(prevView));
  std::memcpy(prevProj, savedPrevProj, sizeof(prevProj));
}
void DeferredRenderer::EndEnvFace(const float camPos[3]) {
  if (camPos == nullptr) {
    captureMode = false;
    captureTarget = 0;
    return;
  }
  float cp[3] = { camPos[0], camPos[1], camPos[2] };
  EndInternal(cp, true);
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
  uint64_t casterHash = CasterHash();
  stats.shadowCached = false;
  if (shadowCacheOn && hasCasterHash && casterHash == lastCasterHash && shadowAtlas != 0 && shadowCube != 0) {
    stats.shadowCached = true;
    return;
  }
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
    lastCasterHash = casterHash;
    hasCasterHash = true;
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
    lastCasterHash = casterHash;
    hasCasterHash = true;
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
    if (res > shadowTileSize) {
      res = shadowTileSize;
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
      float frac = static_cast<float>(res) / static_cast<float>(shadowTileSize);
      shadowCascadeUV[tile] = PCSSOrthoUVScale(ce, frac);
      shadowCascadeK[tile] = frac;
      shadowCascadeSize[tile] = dl.effectiveSize;
      shadowCascadeLight[tile] = static_cast<float>(dirEmission[d]);
      rhi->SetRenderTargets(0, nullptr, shadowAtlas);
      RHIViewport tvp;
      tvp.x = static_cast<float>((tile % 2) * shadowTileSize);
      tvp.y = static_cast<float>((tile / 2) * shadowTileSize);
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
        ++stats.shadowDraws;
        stats.shadowTris += shadowDraws[di].count / 3;
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
    if (res > shadowTileSize) {
      res = shadowTileSize;
    }
    float frac = static_cast<float>(res) / static_cast<float>(shadowTileSize);
    shadowSpotMeta[s][0] = static_cast<float>(tile);
    size_t em = spotEmitBase + spotOrder[s];
    shadowSpotMeta[s][1] = em < 16 ? static_cast<float>(em) : -1.0f;
    shadowSpotMeta[s][2] = sl.effectiveSize;
    shadowSpotMeta[s][3] = frac;
    rhi->SetRenderTargets(0, nullptr, shadowAtlas);
    RHIViewport tvp;
    tvp.x = static_cast<float>((tile % 2) * shadowTileSize);
    tvp.y = static_cast<float>((tile / 2) * shadowTileSize);
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
      ++stats.shadowDraws;
      stats.shadowTris += shadowDraws[di].count / 3;
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
      cvp.w = static_cast<float>(shadowCubeSize);
      cvp.h = static_cast<float>(shadowCubeSize);
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
        ++stats.shadowDraws;
        stats.shadowTris += shadowDraws[di].count / 3;
      }
    }
  }
  shadowMapsValid = shadowCascadeActive > 0 || shadowSpotActive > 0 || shadowPointActive;
  lastCasterHash = casterHash;
  hasCasterHash = true;
}
bool DeferredRenderer::Resize(int nw, int nh) {
  if (nw <= 0 || nh <= 0) {
    return false;
  }
  if (nw == outW && nh == outH) {
    return true;
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
    int nw = static_cast<int>(static_cast<float>(outW) * renderScale);
    int nh = static_cast<int>(static_cast<float>(outH) * renderScale);
    if (nw < 1) {
      nw = 1;
    }
    if (nh < 1) {
      nh = 1;
    }
    if (nw == w && nh == h) {
      return;
    }
    w = nw;
    h = nh;
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
  RHIRenderTarget rt = fsrActive ? fsrOut : gradeA;
  if (filmActive && filmA != 0 && !fsrActive) {
    rt = filmA;
  } else if (!filmActive && fxaaRan && fxaaA != 0 && !fsrActive) {
    rt = fxaaA;
  }
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
  std::memcpy(prevView, lastView, sizeof(prevView));
  std::memcpy(prevProj, lastProj, sizeof(prevProj));
  std::memcpy(lastView, view, sizeof(lastView));
  std::memcpy(lastProj, proj, sizeof(lastProj));
  float vp[16];
  MulViewProj(lastView, lastProj, vp);
  ExtractFrustumPlanes(vp, frustumPlanes);
  frustumValid = true;
  culledDraws = 0;
  PollGpuTimers();
  ++occFrame;
  occBudget = 256;
  PollOcclusion();
  double keepGpu = stats.gpuMs;
  double keepShadow = stats.gpuShadowMs;
  double keepLight = stats.gpuLightMs;
  double keepPost = stats.gpuPostMs;
  bool keepValid = stats.gpuValid;
  stats = RenderStats();
  stats.gpuMs = keepGpu;
  stats.gpuShadowMs = keepShadow;
  stats.gpuLightMs = keepLight;
  stats.gpuPostMs = keepPost;
  stats.gpuValid = keepValid;
  frameStart = std::chrono::steady_clock::now();
  SetupGeometryPass();
  begun = true;
}
bool DeferredRenderer::CullWorldAABB(const float world[16], const float inMin[3], const float inMax[3]) {
  if (!frustumValid || inMin == nullptr || inMax == nullptr) {
    return false;
  }
  float c[3];
  float e[3];
  TransformAABB(world, inMin, inMax, c, e);
  if (CullAABB(frustumPlanes, c, e)) {
    ++culledDraws;
    return true;
  }
  return false;
}
uint32_t DeferredRenderer::CulledDraws() const {
  return culledDraws;
}
const RenderStats& DeferredRenderer::GetStats() const {
  return stats;
}
void DeferredRenderer::SetDynamicResolution(bool on, float targetMs, float minScale, float maxScale) {
  dynResOn = on;
  if (targetMs > 0.0f) {
    dynTargetMs = targetMs;
  }
  if (minScale < 0.25f) {
    minScale = 0.25f;
  }
  if (maxScale > 1.0f) {
    maxScale = 1.0f;
  }
  if (minScale > maxScale) {
    minScale = maxScale;
  }
  dynMin = minScale;
  dynMax = maxScale;
  dynScale = renderScale;
  if (dynScale < dynMin) {
    dynScale = dynMin;
  }
  if (dynScale > dynMax) {
    dynScale = dynMax;
  }
  dynFrames = 0;
  dynAccum = 0.0;
}
float DeferredRenderer::GetDynamicScale() const {
  if (!dynResOn) {
    return renderScale;
  }
  return dynScale;
}
void DeferredRenderer::UpdateDynamicResolution() {
  double signal = stats.gpuValid ? stats.gpuMs : stats.msTotal;
  dynAccum += signal;
  ++dynFrames;
  if (dynFrames < 30) {
    return;
  }
  double avg = dynAccum / static_cast<double>(dynFrames);
  dynFrames = 0;
  dynAccum = 0.0;
  if (avg > static_cast<double>(dynTargetMs) * 1.1 && dynScale > dynMin) {
    dynScale -= 0.05f;
    if (dynScale < dynMin) {
      dynScale = dynMin;
    }
    SetFsr(dynScale, fsrSharpness);
  } else if (avg < static_cast<double>(dynTargetMs) * 0.9 && dynScale < dynMax) {
    dynScale += 0.05f;
    if (dynScale > dynMax) {
      dynScale = dynMax;
    }
    SetFsr(dynScale, fsrSharpness);
  }
}
double DeferredRenderer::ElapsedMs(const std::chrono::steady_clock::time_point& from, const std::chrono::steady_clock::time_point& to) {
  return std::chrono::duration<double, std::milli>(to - from).count();
}
void DeferredRenderer::NoteFullscreen() {
  rhi->DrawFullscreenTriangle();
  ++stats.postPasses;
}
void DeferredRenderer::NoteLighting() {
  rhi->DrawFullscreenTriangle();
  ++stats.lightingDraws;
}
void DeferredRenderer::BeginShadowSection() {
  sectionStart = std::chrono::steady_clock::now();
  if (gpuShadowTs != 0) {
    rhi->BeginTimestamp(gpuShadowTs);
  }
}
void DeferredRenderer::EndShadowSection() {
  stats.msShadow = ElapsedMs(sectionStart, std::chrono::steady_clock::now());
  if (gpuShadowTs != 0) {
    rhi->EndTimestamp(gpuShadowTs);
  }
}
void DeferredRenderer::BeginLightSection() {
  sectionStart = std::chrono::steady_clock::now();
  if (gpuLightTs != 0) {
    rhi->BeginTimestamp(gpuLightTs);
  }
}
void DeferredRenderer::EndLightSection() {
  stats.msLighting = ElapsedMs(sectionStart, std::chrono::steady_clock::now());
  if (gpuLightTs != 0) {
    rhi->EndTimestamp(gpuLightTs);
  }
}
void DeferredRenderer::BeginPostSection() {
  sectionStart = std::chrono::steady_clock::now();
  if (gpuPostTs != 0) {
    rhi->BeginTimestamp(gpuPostTs);
  }
}
void DeferredRenderer::EndPostSection() {
  if (gpuPostTs != 0) {
    rhi->EndTimestamp(gpuPostTs);
  }
}
void DeferredRenderer::PollGpuTimers() {
  if (rhi == nullptr) {
    return;
  }
  double ms = 0.0;
  bool any = false;
  if (gpuShadowTs != 0 && rhi->GetTimestampMs(gpuShadowTs, &ms)) {
    stats.gpuShadowMs = ms;
    any = true;
  }
  if (gpuLightTs != 0 && rhi->GetTimestampMs(gpuLightTs, &ms)) {
    stats.gpuLightMs = ms;
    any = true;
  }
  if (gpuPostTs != 0 && rhi->GetTimestampMs(gpuPostTs, &ms)) {
    stats.gpuPostMs = ms;
    any = true;
  }
  if (any) {
    stats.gpuMs = stats.gpuShadowMs + stats.gpuLightMs + stats.gpuPostMs;
    stats.gpuValid = true;
  }
}
void DeferredRenderer::SetOcclusionCulling(bool on) {
  occOn = on;
}
uint64_t DeferredRenderer::CasterHash() const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&h](const void* data, size_t bytes) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
      h ^= static_cast<uint64_t>(p[i]);
      h *= 1099511628211ull;
    }
  };
  mix(mainCam.camPos, sizeof(mainCam.camPos));
  mix(mainCam.camFwd, sizeof(mainCam.camFwd));
  mix(mainCam.camRight, sizeof(mainCam.camRight));
  mix(mainCam.camUp, sizeof(mainCam.camUp));
  mix(&mainCam.fovY, sizeof(float));
  mix(&mainCam.aspect, sizeof(float));
  mix(&mainCam.nearZ, sizeof(float));
  mix(&mainCam.farZ, sizeof(float));
  mix(lastView, sizeof(lastView));
  mix(lastProj, sizeof(lastProj));
  size_t np = pointLights.size();
  size_t ns = spotLights.size();
  size_t nd = dirLights.size();
  mix(&np, sizeof(np));
  mix(&ns, sizeof(ns));
  mix(&nd, sizeof(nd));
  for (size_t i = 0; i < np; ++i) {
    const RenderPointLight& pl = pointLights[i];
    mix(pl.pos, sizeof(pl.pos));
    mix(pl.color, sizeof(pl.color));
    mix(&pl.intensity, sizeof(float));
    mix(&pl.radius, sizeof(float));
    mix(&pl.castShadow, sizeof(bool));
    mix(&pl.effectiveSize, sizeof(float));
  }
  for (size_t i = 0; i < ns; ++i) {
    const RenderSpotLight& sl = spotLights[i];
    mix(sl.pos, sizeof(sl.pos));
    mix(sl.dir, sizeof(sl.dir));
    mix(sl.color, sizeof(sl.color));
    mix(&sl.intensity, sizeof(float));
    mix(&sl.radius, sizeof(float));
    mix(&sl.angle, sizeof(float));
    mix(&sl.falloff, sizeof(float));
    mix(&sl.castShadow, sizeof(bool));
    mix(&sl.effectiveSize, sizeof(float));
    mix(&sl.shadowSize, sizeof(int));
  }
  for (size_t i = 0; i < nd; ++i) {
    const RenderDirectionalLight& dl = dirLights[i];
    mix(dl.dir, sizeof(dl.dir));
    mix(dl.color, sizeof(dl.color));
    mix(&dl.intensity, sizeof(float));
    mix(&dl.castShadow, sizeof(bool));
    mix(&dl.effectiveSize, sizeof(float));
    mix(&dl.shadowSize, sizeof(int));
    mix(&dl.cascades, sizeof(int));
    mix(&dl.lambda, sizeof(float));
  }
  size_t nsh = shadowDraws.size();
  mix(&nsh, sizeof(nsh));
  for (size_t i = 0; i < nsh; ++i) {
    const ShadowDrawItem& sd = shadowDraws[i];
    mix(sd.world, sizeof(sd.world));
    mix(&sd.vb, sizeof(sd.vb));
    mix(&sd.ib, sizeof(sd.ib));
    mix(&sd.start, sizeof(sd.start));
    mix(&sd.count, sizeof(sd.count));
  }
  return h == 0 ? 1 : h;
}
void DeferredRenderer::SetShadowCaching(bool on) {
  shadowCacheOn = on;
  if (!on) {
    hasCasterHash = false;
  }
}
void DeferredRenderer::SetShadowAutoRes(bool on) {
  shadowAutoRes = on;
  shadowAutoFrames = 0;
  shadowAutoAccum = 0.0;
  shadowStreak = 0;
  shadowLastWant = shadowResLevel;
}
void DeferredRenderer::SetShadowResolution(int atlasSize, int cubeSize) {
  shadowAutoRes = false;
  if (atlasSize < 512) {
    atlasSize = 512;
  }
  if (atlasSize > 4096) {
    atlasSize = 4096;
  }
  if (cubeSize < 128) {
    cubeSize = 128;
  }
  if (cubeSize > 1024) {
    cubeSize = 1024;
  }
  if (atlasSize == shadowAtlasSize && cubeSize == shadowCubeSize) {
    return;
  }
  shadowAtlasSize = atlasSize;
  shadowTileSize = atlasSize / 2;
  shadowCubeSize = cubeSize;
  shadowResLevel = atlasSize >= 4096 ? 0 : (atlasSize >= 2048 ? 1 : 2);
  RecreateShadowTargets();
}
void DeferredRenderer::GetShadowSizes(int& atlasSize, int& cubeSize) const {
  atlasSize = shadowAtlasSize;
  cubeSize = shadowCubeSize;
}
void DeferredRenderer::RecreateShadowTargets() {
  if (rhi == nullptr) {
    return;
  }
  if (shadowAtlas != 0) {
    rhi->DestroyRenderTarget(shadowAtlas);
    shadowAtlas = 0;
  }
  if (shadowCube != 0) {
    rhi->DestroyRenderTarget(shadowCube);
    shadowCube = 0;
  }
  shadowsAvailable = false;
  shadowAtlas = rhi->CreateRenderTarget(shadowAtlasSize, shadowAtlasSize, RHIFormat::R32_DEPTH);
  shadowCube = rhi->CreateShadowCube(shadowCubeSize);
  shadowsAvailable = shadowAtlas != 0 && shadowCube != 0 && shadowSampler != 0;
  hasCasterHash = false;
}
void DeferredRenderer::UpdateShadowAutoRes() {
  if (!shadowAutoRes) {
    return;
  }
  double signal = stats.gpuValid ? stats.gpuMs : stats.msTotal;
  shadowAutoAccum += signal;
  ++shadowAutoFrames;
  if (shadowAutoFrames < 60) {
    return;
  }
  double avg = shadowAutoAccum / static_cast<double>(shadowAutoFrames);
  shadowAutoFrames = 0;
  shadowAutoAccum = 0.0;
  int want = shadowResLevel;
  if (avg > static_cast<double>(dynTargetMs) * 1.2 && shadowResLevel < 2) {
    want = shadowResLevel + 1;
  } else if (avg < static_cast<double>(dynTargetMs) * 0.7 && shadowResLevel > 0) {
    want = shadowResLevel - 1;
  }
  if (want == shadowResLevel) {
    shadowStreak = 0;
    shadowLastWant = want;
    return;
  }
  if (want == shadowLastWant) {
    ++shadowStreak;
  } else {
    shadowStreak = 1;
    shadowLastWant = want;
  }
  if (shadowStreak < 3) {
    return;
  }
  shadowStreak = 0;
  shadowResLevel = want;
  shadowAtlasSize = want == 0 ? 4096 : (want == 1 ? 2048 : 1024);
  shadowTileSize = shadowAtlasSize / 2;
  shadowCubeSize = want == 0 ? 512 : (want == 1 ? 256 : 128);
  RecreateShadowTargets();
}
uint64_t DeferredRenderer::OccKey(RHIBuffer vb, RHIBuffer ib, uint32_t start, uint32_t count, const float* world) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&h](const void* data, size_t bytes) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
      h ^= static_cast<uint64_t>(p[i]);
      h *= 1099511628211ull;
    }
  };
  mix(&vb, sizeof(vb));
  mix(&ib, sizeof(ib));
  mix(&start, sizeof(start));
  mix(&count, sizeof(count));
  if (world != nullptr) {
    mix(world, sizeof(float) * 16);
  }
  return h == 0 ? 1 : h;
}
bool DeferredRenderer::OccDrawBegin(RHIBuffer vb, RHIBuffer ib, uint32_t start, uint32_t count, const float* world) {
  if (!occOn || captureMode || rhi == nullptr) {
    return true;
  }
  uint64_t key = OccKey(vb, ib, start, count, world);
  auto it = occMap.find(key);
  if (it != occMap.end() && it->second.hasResult && it->second.resultFrame + 1 == occFrame && it->second.samples == 0) {
    ++stats.occludedDraws;
    return false;
  }
  if (occBudget <= 0) {
    return true;
  }
  if (it == occMap.end()) {
    SweepOcclusion();
    RHIOcclusionQuery q = rhi->CreateOcclusionQuery();
    if (q == 0) {
      return true;
    }
    OccEntry e;
    e.q = q;
    e.queriedFrame = occFrame;
    occMap[key] = e;
    it = occMap.find(key);
  }
  it->second.queriedFrame = occFrame;
  --occBudget;
  rhi->BeginOcclusionQuery(it->second.q);
  occActive = it->second.q;
  return true;
}
void DeferredRenderer::OccDrawEnd() {
  if (occActive != 0 && rhi != nullptr) {
    rhi->EndOcclusionQuery(occActive);
  }
  occActive = 0;
}
void DeferredRenderer::PollOcclusion() {
  if (rhi == nullptr) {
    return;
  }
  for (auto& kv : occMap) {
    if (kv.second.hasResult && kv.second.resultFrame == kv.second.queriedFrame) {
      continue;
    }
    if (kv.second.queriedFrame >= occFrame) {
      continue;
    }
    uint64_t s = 0;
    if (rhi->GetOcclusionSamples(kv.second.q, &s)) {
      kv.second.samples = s;
      kv.second.hasResult = true;
      kv.second.resultFrame = kv.second.queriedFrame;
    }
  }
  for (auto& kv : occGroups) {
    if (kv.second.hasResult && kv.second.resultFrame == kv.second.queriedFrame) {
      continue;
    }
    if (kv.second.queriedFrame >= occFrame) {
      continue;
    }
    uint64_t s = 0;
    if (rhi->GetOcclusionSamples(kv.second.q, &s)) {
      kv.second.samples = s;
      kv.second.hasResult = true;
      kv.second.resultFrame = kv.second.queriedFrame;
    }
  }
}
void DeferredRenderer::SweepOcclusion() {
  if (occMap.size() <= 1024) {
    return;
  }
  for (auto it = occMap.begin(); it != occMap.end();) {
    if (it->second.queriedFrame + 2 < occFrame) {
      if (it->second.q != 0 && rhi != nullptr) {
        rhi->DestroyOcclusionQuery(it->second.q);
      }
      it = occMap.erase(it);
    } else {
      ++it;
    }
  }
  if (occMap.size() > 1536 && rhi != nullptr) {
    for (auto& kv : occMap) {
      if (kv.second.q != 0) {
        rhi->DestroyOcclusionQuery(kv.second.q);
      }
    }
    occMap.clear();
  }
  if (occGroups.size() > 256) {
    for (auto it = occGroups.begin(); it != occGroups.end();) {
      if (it->second.queriedFrame + 2 < occFrame) {
        if (it->second.q != 0 && rhi != nullptr) {
          rhi->DestroyOcclusionQuery(it->second.q);
        }
        it = occGroups.erase(it);
      } else {
        ++it;
      }
    }
  }
  occActive = 0;
}
void DeferredRenderer::BeginEnvFace(int face, const float view[16], const float proj[16]) {
  begun = false;
  if (!ready || rhi == nullptr || captureCube == 0) {
    return;
  }
  captureTarget = rhi->GetEnvCubeFace(captureCube, face);
  if (captureTarget == 0) {
    return;
  }
  captureMode = true;
  std::memcpy(lastView, view, sizeof(lastView));
  std::memcpy(lastProj, proj, sizeof(lastProj));
  float vp[16];
  MulViewProj(lastView, lastProj, vp);
  ExtractFrustumPlanes(vp, frustumPlanes);
  frustumValid = true;
  culledDraws = 0;
  SetupGeometryPass();
  begun = true;
}
void DeferredRenderer::SetupGeometryPass() {
  shadowDraws.clear();
  decalDraws.clear();
  forwardDraws.clear();
  geoQueue.clear();
  shadowMapsValid = false;
  ssaoValid = false;
  ssrValid = false;
  matTableRows = 1;
  RHIRenderTarget mrts[8] = { gAlbedo, gNormalRough, gMetallic, gPosition, gLightmap, gEmissive, gF0, gExtA };
  rhi->SetRenderTargets(8, mrts, gDepth);
  rhi->ClearRenderTarget(gAlbedo, 0.02f, 0.02f, 0.03f, 0.5f);
  rhi->ClearRenderTarget(gNormalRough, 0.5f, 0.5f, 1.0f, 1.0f);
  rhi->ClearRenderTarget(gMetallic, 0.0f, 0.0f, 0.0f, 0.0f);
  rhi->ClearRenderTarget(gPosition, 0.0f, 0.0f, 0.0f, 0.5f);
  rhi->ClearRenderTarget(gLightmap, 0.0f, 0.0f, 0.0f, 0.0f);
  rhi->ClearRenderTarget(gEmissive, 0.0f, 0.0f, 0.0f, 0.0f);
  rhi->ClearRenderTarget(gF0, 0.04f, 0.04f, 0.04f, 0.0f);
  rhi->ClearRenderTarget(gExtA, 0.0f, 0.0f, 0.0f, 0.0f);
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
  mc.extra[0] = 1.0f;
  mc.extra[1] = 0.0f;
  mc.extra[2] = 0.0f;
  mc.extra[3] = 0.0f;
  rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  rhi->SetPixelConstantBuffer(0, matCB);
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
  if (meshHasBounds && CullWorldAABB(world, meshMin, meshMax)) {
    return;
  }
  if (material.transmission > 0.0f || material.alphaMode == 2) {
    ForwardDrawItem fwd;
    std::memcpy(fwd.world, world, sizeof(fwd.world));
    fwd.vb = vb;
    fwd.ib = ib;
    fwd.start = 0;
    fwd.count = indexCount;
    fwd.mat = material;
    forwardDraws.push_back(fwd);
    return;
  }
  GeoBatchItem gi;
  std::memcpy(gi.world, world, sizeof(gi.world));
  gi.vb = vb;
  gi.ib = ib;
  gi.start = 0;
  gi.count = indexCount;
  gi.mat = material;
  gi.lodFade = 0.0f;
  gi.matHash = MatHash(material);
  geoQueue.push_back(gi);
}
void DeferredRenderer::DrawObjectEx(const float world[16], RHIBuffer vb, RHIBuffer ib, uint32_t start, uint32_t count, const DeferredMaterial* mat, float lodFade, const float* aabbMin, const float* aabbMax) {
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
  if (CullWorldAABB(world, aabbMin, aabbMax)) {
    return;
  }
  const DeferredMaterial& use = (mat != nullptr) ? *mat : material;
  if (use.transmission > 0.0f || use.alphaMode == 2) {
    ForwardDrawItem fwd;
    std::memcpy(fwd.world, world, sizeof(fwd.world));
    fwd.vb = vb;
    fwd.ib = ib;
    fwd.start = start;
    fwd.count = count;
    fwd.mat = use;
    forwardDraws.push_back(fwd);
    return;
  }
  GeoBatchItem gi;
  std::memcpy(gi.world, world, sizeof(gi.world));
  gi.vb = vb;
  gi.ib = ib;
  gi.start = start;
  gi.count = count;
  gi.mat = use;
  gi.lodFade = lodFade;
  gi.matHash = MatHash(use);
  geoQueue.push_back(gi);
}
uint64_t DeferredRenderer::MatHash(const DeferredMaterial& mat) {
  uint64_t h = 1469598103934665603ull;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(&mat);
  for (size_t i = 0; i < sizeof(DeferredMaterial); ++i) {
    h ^= static_cast<uint64_t>(p[i]);
    h *= 1099511628211ull;
  }
  return h == 0 ? 1 : h;
}
uint64_t DeferredRenderer::GeoGroupKey(const GeoBatchItem& item) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&h](const void* data, size_t bytes) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
      h ^= static_cast<uint64_t>(p[i]);
      h *= 1099511628211ull;
    }
  };
  mix(&item.vb, sizeof(item.vb));
  mix(&item.ib, sizeof(item.ib));
  mix(&item.start, sizeof(item.start));
  mix(&item.count, sizeof(item.count));
  mix(&item.matHash, sizeof(item.matHash));
  uint32_t fadeBits = 0;
  std::memcpy(&fadeBits, &item.lodFade, sizeof(fadeBits));
  mix(&fadeBits, sizeof(fadeBits));
  return h == 0 ? 1 : h;
}
void DeferredRenderer::SetGeoMaterial(const DeferredMaterial& use, float lodFade) {
  MatConstants mc;
  MatExtConstants mec;
  uint32_t matRow = 0;
  FillMatConstants(use, lodFade < 0.0f ? 0.0f : (lodFade > 1.0f ? 1.0f : lodFade), mc, mec, matTablePx, matTableRows, matRow);
  (void)matRow;
  rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  rhi->UpdateConstantBuffer(matExtCB, &mec, sizeof(mec));
  rhi->SetPixelConstantBuffer(0, matCB);
  rhi->SetPixelConstantBuffer(1, matExtCB);
  RHITexture alb = (use.texFlags & 1u) != 0 && use.albedoTex != 0 ? use.albedoTex : neutralAlbedo;
  RHITexture nrm = (use.texFlags & 2u) != 0 && use.normalTex != 0 ? use.normalTex : neutralNormal;
  RHITexture orm = (use.texFlags & 4u) != 0 && use.ormTex != 0 ? use.ormTex : neutralOrm;
  RHITexture hgt = (use.texFlags & 8u) != 0 && use.heightTex != 0 ? use.heightTex : 0;
  RHITexture lmt = (use.texFlags & 16u) != 0 && use.lightmapTex != 0 ? use.lightmapTex : 0;
  RHITexture emi = (use.texFlags & 32u) != 0 && use.emissiveTex != 0 ? use.emissiveTex : neutralBlack;
  RHITexture spc = (use.texFlags & 64u) != 0 && use.specularTex != 0 ? use.specularTex : neutralBlack;
  RHITexture spcC = (use.texFlags & 128u) != 0 && use.specularColorTex != 0 ? use.specularColorTex : neutralAlbedo;
  RHITexture ccT = (use.texFlags & 256u) != 0 && use.clearcoatTex != 0 ? use.clearcoatTex : neutralBlack;
  RHITexture ccR = (use.texFlags & 512u) != 0 && use.clearcoatRoughnessTex != 0 ? use.clearcoatRoughnessTex : neutralBlack;
  RHITexture ccN = (use.texFlags & 1024u) != 0 && use.clearcoatNormalTex != 0 ? use.clearcoatNormalTex : neutralNormal;
  RHITexture shC = (use.texFlags & 2048u) != 0 && use.sheenColorTex != 0 ? use.sheenColorTex : neutralBlack;
  RHITexture shR = (use.texFlags & 4096u) != 0 && use.sheenRoughnessTex != 0 ? use.sheenRoughnessTex : neutralBlack;
  RHITexture anT = (use.texFlags & 8192u) != 0 && use.anisotropyTex != 0 ? use.anisotropyTex : neutralAniso;
  RHITexture irT = (use.texFlags & 16384u) != 0 && use.iridescenceTex != 0 ? use.iridescenceTex : neutralBlack;
  RHITexture irH = (use.texFlags & 32768u) != 0 && use.iridescenceThicknessTex != 0 ? use.iridescenceThicknessTex : neutralBlack;
  rhi->SetPixelTexture2D(0, alb);
  rhi->SetPixelTexture2D(1, nrm);
  rhi->SetPixelTexture2D(2, orm);
  rhi->SetPixelTexture2D(3, hgt);
  rhi->SetPixelTexture2D(4, lmt);
  rhi->SetPixelTexture2D(5, emi);
  rhi->SetPixelTexture2D(6, spc);
  rhi->SetPixelTexture2D(7, spcC);
  rhi->SetPixelTexture2D(8, ccT);
  rhi->SetPixelTexture2D(9, ccR);
  rhi->SetPixelTexture2D(10, ccN);
  rhi->SetPixelTexture2D(11, shC);
  rhi->SetPixelTexture2D(12, shR);
  rhi->SetPixelTexture2D(13, anT);
  rhi->SetPixelTexture2D(14, irT);
  rhi->SetPixelTexture2D(15, irH);
  rhi->SetPixelSampler(0, geoSampler);
  rhi->SetPixelSampler(1, geoSampler);
  rhi->SetPixelSampler(2, geoSampler);
  rhi->SetPixelSampler(3, geoSampler);
  rhi->SetPixelSampler(4, geoSampler);
  if (use.doubleSided) {
    RHIRasterizer rs;
    rs.cull = RHICull::None;
    rs.fill = RHIFill::Solid;
    rs.frontCCW = false;
    rs.slopeBias = 0.0f;
    rhi->SetRasterizerState(rs);
  } else {
    RHIRasterizer rs;
    rs.cull = RHICull::Back;
    rs.fill = RHIFill::Solid;
    rs.frontCCW = false;
    rs.slopeBias = 0.0f;
    rhi->SetRasterizerState(rs);
  }
}
void DeferredRenderer::DrawGeoSingle(const GeoBatchItem& item) {
  if (rhi == nullptr) {
    return;
  }
  const DeferredMaterial& use = item.mat;
  GeoConstants gc;
  std::memcpy(gc.world, item.world, sizeof(gc.world));
  std::memcpy(gc.view, lastView, sizeof(gc.view));
  std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
  rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
  rhi->SetVertexShader(geoVS);
  rhi->SetInputLayout(layout);
  rhi->SetPixelShader(geoPS);
  rhi->SetVertexConstantBuffer(0, geoCB);
  SetGeoMaterial(use, item.lodFade);
  rhi->SetVertexBuffer(item.vb, 0);
  rhi->SetIndexBuffer(item.ib);
  if (OccDrawBegin(item.vb, item.ib, item.start, item.count, item.world)) {
    rhi->DrawIndexed(item.count, item.start, 0);
    OccDrawEnd();
    ++stats.geometryDraws;
    stats.geometryTris += item.count / 3;
  }
}
void DeferredRenderer::DrawGeoInstanced(uint64_t key, const std::vector<size_t>& indices) {
  if (rhi == nullptr || indices.empty()) {
    return;
  }
  if (!batchOk || indices.size() < 2) {
    for (size_t i : indices) {
      DrawGeoSingle(geoQueue[i]);
    }
    return;
  }
  const GeoBatchItem& first = geoQueue[indices[0]];
  uint64_t mh = 1469598103934665603ull;
  for (size_t i : indices) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(geoQueue[i].world);
    for (size_t b = 0; b < sizeof(float) * 16; ++b) {
      mh ^= static_cast<uint64_t>(p[b]);
      mh *= 1099511628211ull;
    }
  }
  if (mh == 0) {
    mh = 1;
  }
  auto git = occGroups.find(key);
  if (occOn && !captureMode && git != occGroups.end() && git->second.hasResult && git->second.resultFrame + 1 == occFrame && git->second.memberHash == mh && git->second.samples == 0) {
    stats.occludedDraws += static_cast<uint32_t>(indices.size());
    return;
  }
  if (git == occGroups.end() && !captureMode && occOn) {
    RHIOcclusionQuery q = rhi->CreateOcclusionQuery();
    if (q == 0) {
      for (size_t i : indices) {
        DrawGeoSingle(geoQueue[i]);
      }
      return;
    }
    OccGroup g;
    g.q = q;
    occGroups[key] = g;
    git = occGroups.find(key);
  }
  SetGeoMaterial(first.mat, first.lodFade);
  GeoConstants gc;
  float identity[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  std::memcpy(gc.world, identity, sizeof(gc.world));
  std::memcpy(gc.view, lastView, sizeof(gc.view));
  std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
  rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
  rhi->SetVertexShader(geoVSInstanced);
  rhi->SetInputLayout(instLayout);
  rhi->SetPixelShader(geoPS);
  rhi->SetVertexConstantBuffer(0, geoCB);
  rhi->SetVertexBuffer(first.vb, 0);
  rhi->SetIndexBuffer(first.ib);
  uint64_t need = static_cast<uint64_t>(indices.size()) * 64;
  if (instVB == 0 || instCap < need) {
    if (instVB != 0) {
      rhi->DestroyBuffer(instVB);
      instVB = 0;
    }
    uint64_t alloc = need < 64 ? 64 : need;
    instVB = rhi->CreateBufferEmpty(alloc, 64, false);
    instCap = instVB != 0 ? alloc : 0;
    instHasLast = false;
  }
  if (instVB == 0) {
    for (size_t i : indices) {
      DrawGeoSingle(geoQueue[i]);
    }
    return;
  }
  if (!instHasLast || instLastKey != key || instLastHash != mh) {
    std::vector<unsigned char> bytes(static_cast<size_t>(need));
    for (size_t k = 0; k < indices.size(); ++k) {
      std::memcpy(bytes.data() + k * 64, geoQueue[indices[k]].world, 64);
    }
    if (!rhi->UpdateBufferDiscard(instVB, bytes.data(), static_cast<size_t>(need))) {
      for (size_t i : indices) {
        DrawGeoSingle(geoQueue[i]);
      }
      return;
    }
    instHasLast = true;
    instLastKey = key;
    instLastHash = mh;
  }
  rhi->SetInstanceBuffer(instVB);
  if (occOn && !captureMode && git != occGroups.end() && occBudget > 0) {
    --occBudget;
    git->second.memberHash = mh;
    git->second.queriedFrame = occFrame;
    rhi->BeginOcclusionQuery(git->second.q);
    rhi->DrawIndexedInstanced(first.count, first.start, 0, static_cast<uint32_t>(indices.size()));
    rhi->EndOcclusionQuery(git->second.q);
  } else {
    rhi->DrawIndexedInstanced(first.count, first.start, 0, static_cast<uint32_t>(indices.size()));
  }
  stats.geometryDraws += static_cast<uint32_t>(indices.size());
  stats.geometryTris += static_cast<uint64_t>(first.count / 3) * static_cast<uint64_t>(indices.size());
  ++stats.instancedDraws;
}
void DeferredRenderer::FlushGeoQueue() {
  if (rhi == nullptr || geoQueue.empty()) {
    geoQueue.clear();
    return;
  }
  std::unordered_map<uint64_t, std::vector<size_t>> groups;
  std::vector<uint64_t> order;
  for (size_t i = 0; i < geoQueue.size(); ++i) {
    uint64_t key = GeoGroupKey(geoQueue[i]);
    auto it = groups.find(key);
    if (it == groups.end()) {
      groups[key] = std::vector<size_t>();
      order.push_back(key);
    }
    groups[key].push_back(i);
  }
  for (size_t o = 0; o < order.size(); ++o) {
    const std::vector<size_t>& idx = groups[order[o]];
    if (idx.size() < 2 || !batchOk) {
      for (size_t i : idx) {
        DrawGeoSingle(geoQueue[i]);
      }
    } else {
      DrawGeoInstanced(order[o], idx);
    }
  }
  geoQueue.clear();
}
void DeferredRenderer::DrawDecal(const float world[16], const float invWorld[16], const float size[3], float fade, RHITexture tex) {
  if (!begun || world == nullptr || invWorld == nullptr || size == nullptr || tex == 0) {
    return;
  }
  if (fade <= 0.0f || size[0] <= 0.0f || size[1] <= 0.0f || size[2] <= 0.0f) {
    return;
  }
  DecalDrawItem item;
  std::memcpy(item.world, world, sizeof(item.world));
  std::memcpy(item.invWorld, invWorld, sizeof(item.invWorld));
  item.size[0] = size[0];
  item.size[1] = size[1];
  item.size[2] = size[2];
  item.fade = fade > 1.0f ? 1.0f : fade;
  item.tex = tex;
  decalDraws.push_back(item);
}
void DeferredRenderer::RenderDecals() {
  if (decalDraws.empty() || decalVB == 0 || decalIB == 0 || decalVS == 0 || decalPS == 0) {
    decalDraws.clear();
    return;
  }
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
  rs.cull = RHICull::None;
  rs.fill = RHIFill::Solid;
  rs.frontCCW = false;
  rs.slopeBias = 0.0f;
  rhi->SetRasterizerState(rs);
  RHIDepthStencil ds;
  ds.depthEnable = true;
  ds.depthWrite = false;
  rhi->SetDepthStencilState(ds);
  RHIBlend blend;
  blend.enable = true;
  rhi->SetBlendState(blend);
  rhi->SetInputLayout(layout);
  rhi->SetVertexShader(decalVS);
  rhi->SetPixelShader(decalPS);
  rhi->SetVertexBuffer(decalVB, 0);
  rhi->SetIndexBuffer(decalIB);
  rhi->SetVertexConstantBuffer(0, geoCB);
  rhi->SetPixelConstantBuffer(0, decalCB);
  rhi->SetPixelSampler(0, geoSampler);
  rhi->SetRenderTargets(1, &gAlbedo, gDepth);
  for (size_t i = 0; i < decalDraws.size(); ++i) {
    const DecalDrawItem& d = decalDraws[i];
    GeoConstants gc;
    std::memcpy(gc.world, d.world, sizeof(gc.world));
    std::memcpy(gc.view, lastView, sizeof(gc.view));
    std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
    rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
    float dc[16];
    dc[0] = d.invWorld[0];
    dc[1] = d.invWorld[1];
    dc[2] = d.invWorld[2];
    dc[3] = d.invWorld[3];
    dc[4] = d.invWorld[4];
    dc[5] = d.invWorld[5];
    dc[6] = d.invWorld[6];
    dc[7] = d.invWorld[7];
    dc[8] = d.invWorld[8];
    dc[9] = d.invWorld[9];
    dc[10] = d.invWorld[10];
    dc[11] = d.invWorld[11];
    dc[12] = d.size[0];
    dc[13] = d.size[1];
    dc[14] = d.size[2];
    dc[15] = d.fade;
    rhi->UpdateConstantBuffer(decalCB, &dc, sizeof(dc));
    rhi->SetPixelTexture2D(0, d.tex);
    rhi->DrawIndexed(36, 0, 0);
    ++stats.decalDraws;
  }
  rhi->SetPixelTexture2D(0, 0);
  blend.enable = false;
  rhi->SetBlendState(blend);
  ds.depthWrite = true;
  rhi->SetDepthStencilState(ds);
  decalDraws.clear();
}
void DeferredRenderer::RenderForwardPass(const float camPos[3]) {
  if (forwardDraws.empty() || forwardPS == 0 || copyPS == 0) {
    forwardDraws.clear();
    return;
  }
  std::vector<size_t> order(forwardDraws.size());
  for (size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  float fwd[3] = { mainCam.camFwd[0], mainCam.camFwd[1], mainCam.camFwd[2] };
  for (size_t i = 0; i < order.size(); ++i) {
    for (size_t j = i + 1; j < order.size(); ++j) {
      const ForwardDrawItem& a = forwardDraws[order[i]];
      const ForwardDrawItem& b = forwardDraws[order[j]];
      float dax = a.world[12] - camPos[0];
      float day = a.world[13] - camPos[1];
      float daz = a.world[14] - camPos[2];
      float dbx = b.world[12] - camPos[0];
      float dby = b.world[13] - camPos[1];
      float dbz = b.world[14] - camPos[2];
      float da = dax * fwd[0] + day * fwd[1] + daz * fwd[2];
      float db = dbx * fwd[0] + dby * fwd[1] + dbz * fwd[2];
      if (db > da) {
        size_t t = order[i];
        order[i] = order[j];
        order[j] = t;
      }
    }
  }
  rhi->SetRenderTargets(1, &transSrc, 0);
  RHIViewport cvp;
  cvp.x = viewX;
  cvp.y = viewY;
  cvp.w = static_cast<float>(w);
  cvp.h = static_cast<float>(h);
  cvp.minD = 0.0f;
  cvp.maxD = 1.0f;
  rhi->SetViewport(cvp);
  rhi->SetVertexShader(lightVS);
  rhi->SetPixelShader(copyPS);
  rhi->SetPixelTexture(0, gLight);
  rhi->SetPixelSampler(0, sampler);
  NoteFullscreen();
  rhi->SetPixelTexture(0, 0);
  RHIRenderTarget fwds[1] = { gLight };
  rhi->SetRenderTargets(1, fwds, gDepth);
  rhi->SetViewport(cvp);
  rhi->SetTopology(RHITopology::TriangleList);
  RHIDepthStencil ds;
  ds.depthEnable = true;
  ds.depthWrite = false;
  rhi->SetDepthStencilState(ds);
  RHIBlend blend;
  blend.enable = true;
  rhi->SetBlendState(blend);
  rhi->SetVertexShader(geoVS);
  rhi->SetPixelShader(forwardPS);
  rhi->SetVertexConstantBuffer(0, geoCB);
  rhi->SetPixelConstantBuffer(0, lightCB);
  float fwdScreen[4] = { static_cast<float>(w), static_cast<float>(h), 0.0f, 0.0f };
  rhi->UpdateConstantBuffer(forwardCB, &fwdScreen, sizeof(fwdScreen));
  rhi->SetPixelConstantBuffer(0, lightCB);
  rhi->SetPixelConstantBuffer(3, forwardCB);
  rhi->SetPixelTexture(16, transSrc);
  rhi->SetPixelTexture2D(17, envCube);
  rhi->SetPixelSampler(0, sampler);
  for (size_t oi = 0; oi < order.size(); ++oi) {
    const ForwardDrawItem& d = forwardDraws[order[oi]];
    GeoConstants gc;
    std::memcpy(gc.world, d.world, sizeof(gc.world));
    std::memcpy(gc.view, lastView, sizeof(gc.view));
    std::memcpy(gc.proj, lastProj, sizeof(gc.proj));
    rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
    MatConstants mc;
    MatExtConstants mec;
    uint32_t matRow = 0;
    FillMatConstants(d.mat, 0.0f, mc, mec, matTablePx, matTableRows, matRow);
    (void)matRow;
    rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
    rhi->UpdateConstantBuffer(matExtCB, &mec, sizeof(mec));
    rhi->SetPixelConstantBuffer(1, matCB);
    rhi->SetPixelConstantBuffer(2, matExtCB);
    RHITexture alb = (d.mat.texFlags & 1u) != 0 && d.mat.albedoTex != 0 ? d.mat.albedoTex : neutralAlbedo;
    RHITexture nrm = (d.mat.texFlags & 2u) != 0 && d.mat.normalTex != 0 ? d.mat.normalTex : neutralNormal;
    RHITexture orm = (d.mat.texFlags & 4u) != 0 && d.mat.ormTex != 0 ? d.mat.ormTex : neutralOrm;
    RHITexture lmt = (d.mat.texFlags & 16u) != 0 && d.mat.lightmapTex != 0 ? d.mat.lightmapTex : 0;
    RHITexture emi = (d.mat.texFlags & 32u) != 0 && d.mat.emissiveTex != 0 ? d.mat.emissiveTex : neutralBlack;
    RHITexture trm = d.mat.transmissionTex != 0 ? d.mat.transmissionTex : neutralBlack;
    RHITexture thm = d.mat.thicknessTex != 0 ? d.mat.thicknessTex : neutralBlack;
    rhi->SetPixelTexture2D(0, alb);
    rhi->SetPixelTexture2D(1, nrm);
    rhi->SetPixelTexture2D(2, orm);
    rhi->SetPixelTexture2D(4, lmt);
    rhi->SetPixelTexture2D(5, emi);
    rhi->SetPixelTexture2D(6, trm);
    rhi->SetPixelTexture2D(7, thm);
    RHIRasterizer rs;
    rs.cull = d.mat.doubleSided ? RHICull::None : RHICull::Back;
    rs.fill = RHIFill::Solid;
    rs.frontCCW = false;
    rs.slopeBias = 0.0f;
    rhi->SetRasterizerState(rs);
    rhi->SetVertexBuffer(d.vb, 0);
    rhi->SetIndexBuffer(d.ib);
    if (OccDrawBegin(d.vb, d.ib, d.start, d.count, d.world)) {
      rhi->DrawIndexed(d.count, d.start, 0);
      OccDrawEnd();
      ++stats.forwardDraws;
      stats.forwardTris += d.count / 3;
    }
  }
  rhi->SetPixelTexture2D(0, 0);
  rhi->SetPixelTexture2D(1, 0);
  rhi->SetPixelTexture2D(2, 0);
  rhi->SetPixelTexture2D(4, 0);
  rhi->SetPixelTexture2D(5, 0);
  rhi->SetPixelTexture2D(6, 0);
  rhi->SetPixelTexture2D(7, 0);
  rhi->SetPixelTexture(16, 0);
  rhi->SetPixelTexture2D(17, 0);
  blend.enable = false;
  rhi->SetBlendState(blend);
  ds.depthWrite = true;
  rhi->SetDepthStencilState(ds);
  forwardDraws.clear();
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
  stats.msSubmit = ElapsedMs(frameStart, std::chrono::steady_clock::now());
  FlushGeoQueue();
  if (!captureMode) {
    BeginShadowSection();
    RenderShadowMaps();
    EndShadowSection();
  }
  auto decalStart = std::chrono::steady_clock::now();
  RenderDecals();
  stats.msDecal = ElapsedMs(decalStart, std::chrono::steady_clock::now());
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
  if (!captureMode && toTexture && ssrOn && ssrA != 0 && ssrPS != 0) {
    SsrConstants sc;
    std::memcpy(sc.viewProj, ssaoVP, sizeof(sc.viewProj));
    sc.camPos[0] = camPos[0];
    sc.camPos[1] = camPos[1];
    sc.camPos[2] = camPos[2];
    sc.camPos[3] = 1.0f;
    sc.params[0] = 24.0f;
    sc.params[1] = 30.0f;
    sc.params[2] = 0.6f;
    sc.params[3] = 1.0f;
    sc.screen[0] = static_cast<float>(w);
    sc.screen[1] = static_cast<float>(h);
    sc.screen[2] = 0.0f;
    sc.screen[3] = 0.0f;
    rhi->UpdateConstantBuffer(ssrCB, &sc, sizeof(sc));
    rhi->SetPixelConstantBuffer(0, ssrCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetRenderTargets(1, &ssrA, 0);
    RHIViewport svp;
    svp.x = 0.0f;
    svp.y = 0.0f;
    svp.w = static_cast<float>(ssrW);
    svp.h = static_cast<float>(ssrH);
    svp.minD = 0.0f;
    svp.maxD = 1.0f;
    rhi->SetViewport(svp);
    rhi->SetVertexShader(lightVS);
    rhi->SetPixelShader(ssrPS);
    rhi->SetPixelTexture(0, gPosition);
    rhi->SetPixelTexture(1, gNormalRough);
    rhi->SetPixelTexture(2, gAlbedo);
    NoteFullscreen();
    ssrValid = true;
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    rhi->SetPixelTexture(2, 0);
  } else if (ssrA != 0) {
    rhi->ClearRenderTarget(ssrA, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  if (!captureMode && toTexture && contactOn && contactA != 0 && contactPS != 0) {
    SsrConstants cc;
    std::memcpy(cc.viewProj, ssaoVP, sizeof(cc.viewProj));
    cc.camPos[0] = camPos[0];
    cc.camPos[1] = camPos[1];
    cc.camPos[2] = camPos[2];
    cc.camPos[3] = 1.0f;
    cc.params[0] = 4.0f;
    cc.params[1] = 0.4f;
    cc.params[2] = 8.0f;
    cc.params[3] = 1.0f;
    cc.screen[0] = static_cast<float>(w);
    cc.screen[1] = static_cast<float>(h);
    cc.screen[2] = 0.0f;
    cc.screen[3] = 0.0f;
    rhi->UpdateConstantBuffer(ssrCB, &cc, sizeof(cc));
    rhi->SetPixelConstantBuffer(0, ssrCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetRenderTargets(1, &contactA, 0);
    RHIViewport cvp;
    cvp.x = 0.0f;
    cvp.y = 0.0f;
    cvp.w = static_cast<float>(w);
    cvp.h = static_cast<float>(h);
    cvp.minD = 0.0f;
    cvp.maxD = 1.0f;
    rhi->SetViewport(cvp);
    rhi->SetVertexShader(lightVS);
    rhi->SetPixelShader(contactPS);
    rhi->SetPixelTexture(0, gPosition);
    rhi->SetPixelTexture(1, gNormalRough);
    NoteFullscreen();
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
  } else if (contactA != 0) {
    rhi->ClearRenderTarget(contactA, 1.0f, 1.0f, 1.0f, 1.0f);
  }
  if (!captureMode && toTexture && ssaoOn && ssaoA != 0 && ssaoB != 0) {
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
    NoteFullscreen();
    float bh[4] = { 1.0f / static_cast<float>(ssaoW), 0.0f, ssaoRadius, 0.0f };
    rhi->UpdateConstantBuffer(ssaoCB, &bh, sizeof(bh));
    rhi->SetRenderTargets(1, &ssaoB, 0);
    rhi->SetPixelShader(ssaoBlurPS);
    rhi->SetPixelTexture(0, ssaoA);
    rhi->SetPixelTexture(1, gPosition);
    rhi->SetPixelTexture(2, gNormalRough);
    NoteFullscreen();
    float bv[4] = { 0.0f, 1.0f / static_cast<float>(ssaoH), ssaoRadius, 0.0f };
    rhi->UpdateConstantBuffer(ssaoCB, &bv, sizeof(bv));
    rhi->SetRenderTargets(1, &ssaoA, 0);
    rhi->SetPixelTexture(0, ssaoB);
    rhi->SetPixelTexture(1, gPosition);
    rhi->SetPixelTexture(2, gNormalRough);
    NoteFullscreen();
    ssaoValid = true;
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    rhi->SetPixelTexture(2, 0);
  }
  if (captureMode && captureTarget != 0) {
    rhi->SetRenderTargets(1, &captureTarget, 0);
  } else if (toTexture) {
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
  rhi->SetPixelTexture(15, gEmissive);
  rhi->SetPixelTexture(16, gF0);
  rhi->SetPixelTexture(17, gExtA);
  rhi->UpdateTextureMip(matTable, 0, 256, 1, 256 * 4, matTablePx, sizeof(matTablePx));
  rhi->SetPixelTexture2D(18, matTable);
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
  lc.shadowInfo[3] = 1.0f / static_cast<float>(shadowAtlasSize);
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
  lc.gradeInfo[2] = envIntensity;
  lc.gradeInfo[3] = envCube != 0 ? 1.0f : 0.0f;
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
  for (int pi = 0; pi < 4; ++pi) {
    lc.probeBoxMin[pi][0] = probeBoxMin[pi][0];
    lc.probeBoxMin[pi][1] = probeBoxMin[pi][1];
    lc.probeBoxMin[pi][2] = probeBoxMin[pi][2];
    lc.probeBoxMin[pi][3] = pi < probeCount ? probeBoxMin[pi][3] : 0.0f;
    lc.probeBoxMax[pi][0] = probeBoxMax[pi][0];
    lc.probeBoxMax[pi][1] = probeBoxMax[pi][1];
    lc.probeBoxMax[pi][2] = probeBoxMax[pi][2];
    lc.probeBoxMax[pi][3] = 0.0f;
  }
  BeginLightSection();
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
  if (ssrValid) {
    rhi->SetPixelTexture(7, ssrA);
  }
  rhi->SetPixelTexture(8, contactA);
  if (captureMode) {
    rhi->SetPixelTexture2D(9, 0);
  } else {
    rhi->SetPixelTexture2D(9, envCube);
  }
  rhi->SetPixelTexture(10, gLightmap);
  rhi->SetPixelTexture2D(11, probeCubes[0]);
  rhi->SetPixelTexture2D(12, probeCubes[1]);
  rhi->SetPixelTexture2D(13, probeCubes[2]);
  rhi->SetPixelTexture2D(14, probeCubes[3]);
  NoteLighting();
  EndLightSection();
  BeginPostSection();
  rhi->SetPixelTexture(0, 0);
  rhi->SetPixelTexture(1, 0);
  rhi->SetPixelTexture(2, 0);
  rhi->SetPixelTexture(3, 0);
  rhi->SetPixelTexture(4, 0);
  rhi->SetPixelTexture(5, 0);
  rhi->SetPixelTexture(6, 0);
  rhi->SetPixelTexture(7, 0);
  rhi->SetPixelTexture(8, 0);
  rhi->SetPixelTexture2D(9, 0);
  rhi->SetPixelTexture(10, 0);
  rhi->SetPixelTexture2D(11, 0);
  rhi->SetPixelTexture2D(12, 0);
  rhi->SetPixelTexture2D(13, 0);
  rhi->SetPixelTexture2D(14, 0);
  rhi->SetPixelTexture(15, 0);
  rhi->SetPixelTexture(16, 0);
  rhi->SetPixelTexture(17, 0);
  rhi->SetPixelTexture2D(18, 0);
  if (toTexture && !captureMode) {
    auto fwdStart = std::chrono::steady_clock::now();
    RenderForwardPass(camPos);
    stats.msForward = ElapsedMs(fwdStart, std::chrono::steady_clock::now());
  } else {
    forwardDraws.clear();
  }
  if (captureMode) {
    captureMode = false;
    captureTarget = 0;
    EndPostSection();
    stats.msTotal = ElapsedMs(frameStart, std::chrono::steady_clock::now());
    stats.culledDraws = culledDraws;
    return;
  }
  if (toTexture && ssgiOn && ssgiA != 0 && ssgiPS != 0) {
    SsrConstants sc;
    std::memcpy(sc.viewProj, ssaoVP, sizeof(sc.viewProj));
    sc.camPos[0] = camPos[0];
    sc.camPos[1] = camPos[1];
    sc.camPos[2] = camPos[2];
    sc.camPos[3] = 1.0f;
    sc.params[0] = 1.0f;
    sc.params[1] = 1.5f;
    sc.params[2] = 0.0f;
    sc.params[3] = 1.0f;
    sc.screen[0] = static_cast<float>(w);
    sc.screen[1] = static_cast<float>(h);
    sc.screen[2] = 0.0f;
    sc.screen[3] = 0.0f;
    rhi->UpdateConstantBuffer(ssrCB, &sc, sizeof(sc));
    rhi->SetPixelConstantBuffer(0, ssrCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetRenderTargets(1, &ssgiA, 0);
    RHIViewport gvp;
    gvp.x = 0.0f;
    gvp.y = 0.0f;
    gvp.w = static_cast<float>(w);
    gvp.h = static_cast<float>(h);
    gvp.minD = 0.0f;
    gvp.maxD = 1.0f;
    rhi->SetViewport(gvp);
    rhi->SetVertexShader(lightVS);
    rhi->SetPixelShader(ssgiPS);
    rhi->SetPixelTexture(0, gPosition);
    rhi->SetPixelTexture(1, gNormalRough);
    rhi->SetPixelTexture(2, gAlbedo);
    rhi->SetPixelTexture(3, gLight);
    NoteFullscreen();
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    rhi->SetPixelTexture(2, 0);
    rhi->SetPixelTexture(3, 0);
    float bh[4] = { 1.0f / static_cast<float>(w), 0.0f, 1.5f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bh, sizeof(bh));
    rhi->SetPixelConstantBuffer(0, bloomCB);
    rhi->SetRenderTargets(1, &ssgiB, 0);
    rhi->SetPixelShader(ssgiBlurPS);
    rhi->SetPixelTexture(0, ssgiA);
    rhi->SetPixelTexture(1, gPosition);
    rhi->SetPixelTexture(2, gNormalRough);
    NoteFullscreen();
    float bv[4] = { 0.0f, 1.0f / static_cast<float>(h), 1.5f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bv, sizeof(bv));
    rhi->SetRenderTargets(1, &ssgiA, 0);
    rhi->SetPixelTexture(0, ssgiB);
    NoteFullscreen();
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    rhi->SetPixelTexture(2, 0);
  } else if (ssgiA != 0) {
    rhi->ClearRenderTarget(ssgiA, 0.0f, 0.0f, 0.0f, 0.0f);
  }
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
    NoteFullscreen();
    float bh[4] = { 1.0f / static_cast<float>(bloomW), 0.0f, 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bh, sizeof(bh));
    rhi->SetRenderTargets(1, &bloomB, 0);
    rhi->SetPixelShader(bloomBlurPS);
    rhi->SetPixelTexture(0, bloomA);
    NoteFullscreen();
    float bv[4] = { 0.0f, 1.0f / static_cast<float>(bloomH), 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(bloomCB, &bv, sizeof(bv));
    rhi->SetRenderTargets(1, &bloomA, 0);
    rhi->SetPixelTexture(0, bloomB);
    NoteFullscreen();
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
    NoteFullscreen();
    rhi->SetPixelTexture(0, 0);
    rhi->SetPixelTexture(1, 0);
    float gd[4] = { exposure, static_cast<float>(toneMode), 0.0f, 0.0f };
    rhi->UpdateConstantBuffer(gradeCB, &gd, sizeof(gd));
    rhi->SetPixelConstantBuffer(0, gradeCB);
    rhi->SetPixelSampler(0, sampler);
    rhi->SetRenderTargets(1, &gradeA, 0);
    rhi->SetViewport(fvp);
    rhi->SetVertexShader(lightVS);
    rhi->SetPixelShader(gradePS);
    rhi->SetPixelTexture(0, gViewport);
    NoteFullscreen();
    rhi->SetPixelTexture(0, 0);
    filmActive = false;
    fxaaRan = false;
    RHIRenderTarget filmSrc = gradeA;
    if (fxaaOn && fxaaA != 0 && fxaaPS != 0) {
      float fx[4] = { 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), static_cast<float>(w), static_cast<float>(h) };
      rhi->UpdateConstantBuffer(fxaaCB, &fx, sizeof(fx));
      rhi->SetPixelConstantBuffer(0, fxaaCB);
      rhi->SetPixelSampler(0, sampler);
      rhi->SetRenderTargets(1, &fxaaA, 0);
      RHIViewport xvp;
      xvp.x = viewX;
      xvp.y = viewY;
      xvp.w = static_cast<float>(w);
      xvp.h = static_cast<float>(h);
      xvp.minD = 0.0f;
      xvp.maxD = 1.0f;
      rhi->SetViewport(xvp);
      rhi->SetPixelShader(fxaaPS);
      rhi->SetPixelTexture(0, gradeA);
      NoteFullscreen();
      rhi->SetPixelTexture(0, 0);
      filmSrc = fxaaA;
      fxaaRan = true;
    }
    if (filmPS != 0 && filmA != 0 && (dofOn || motionOn || filmCa > 0.0f || filmVig > 0.0f || filmGrain > 0.0f || ssgiOn)) {
      filmActive = true;
      PostConstants pc;
      for (int cc = 0; cc < 4; ++cc) {
        for (int rr = 0; rr < 4; ++rr) {
          float s = 0.0f;
          for (int kk = 0; kk < 4; ++kk) {
            s += prevView[cc * 4 + kk] * prevProj[kk * 4 + rr];
          }
          pc.prevVP[cc * 4 + rr] = s;
        }
      }
      pc.camPos[0] = camPos[0];
      pc.camPos[1] = camPos[1];
      pc.camPos[2] = camPos[2];
      pc.camPos[3] = 1.0f;
      pc.focus[0] = focusDist;
      pc.focus[1] = focusRange;
      pc.focus[2] = static_cast<float>(w);
      pc.focus[3] = static_cast<float>(h);
      pc.film[0] = static_cast<float>(frameIndex % 4096);
      pc.film[1] = filmCa;
      pc.film[2] = filmVig;
      pc.film[3] = filmGrain;
      pc.toggles[0] = dofOn ? 1.0f : 0.0f;
      pc.toggles[1] = motionOn ? 1.0f : 0.0f;
      pc.toggles[2] = motionScale;
      pc.toggles[3] = ssgiOn ? 1.0f : 0.0f;
      rhi->UpdateConstantBuffer(filmCB, &pc, sizeof(pc));
      rhi->SetPixelConstantBuffer(0, filmCB);
      rhi->SetPixelSampler(0, sampler);
      rhi->SetRenderTargets(1, &filmA, 0);
      RHIViewport pvp;
      pvp.x = viewX;
      pvp.y = viewY;
      pvp.w = static_cast<float>(w);
      pvp.h = static_cast<float>(h);
      pvp.minD = 0.0f;
      pvp.maxD = 1.0f;
      rhi->SetViewport(pvp);
      rhi->SetPixelShader(filmPS);
      rhi->SetPixelTexture(0, filmSrc);
      rhi->SetPixelTexture(1, gPosition);
      rhi->SetPixelTexture(2, ssgiA);
      NoteFullscreen();
      rhi->SetPixelTexture(0, 0);
      rhi->SetPixelTexture(1, 0);
      rhi->SetPixelTexture(2, 0);
      filmSrc = filmA;
    }
    ++frameIndex;
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
      rhi->SetPixelTexture(0, filmSrc);
      NoteFullscreen();
      AU1 rcon[4];
      FsrRcasCon(rcon, (AF1)((1.0f - fsrSharpness) * 2.0f));
      uint32_t rcas[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      std::memcpy(&rcas[0], rcon, sizeof(uint32_t) * 4);
      rhi->UpdateConstantBuffer(fsrCB, &rcas, sizeof(rcas));
      rhi->SetRenderTargets(1, &fsrOut, 0);
      rhi->SetPixelShader(fsrRcasPS);
      rhi->SetPixelTexture(0, fsrA);
      NoteFullscreen();
      rhi->SetPixelTexture(0, 0);
    }
    EndPostSection();
    auto frameEnd = std::chrono::steady_clock::now();
    stats.msTotal = ElapsedMs(frameStart, frameEnd);
    double parts = stats.msSubmit + stats.msShadow + stats.msDecal + stats.msLighting + stats.msForward;
    stats.msPost = stats.msTotal > parts ? stats.msTotal - parts : 0.0;
    stats.culledDraws = culledDraws;
    uint64_t texB = 0;
    uint64_t tgtB = 0;
    uint64_t bufB = 0;
    rhi->GetMemoryStats(texB, tgtB, bufB);
    stats.textureBytes = texB;
    stats.targetBytes = tgtB;
    stats.bufferBytes = bufB;
    if (dynResOn) {
      UpdateDynamicResolution();
    }
    UpdateShadowAutoRes();
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
  gLightmap = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gEmissive = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gF0 = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gExtA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  transSrc = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gDepth = rhi->CreateRenderTarget(w, h, RHIFormat::D24S8);
  gViewport = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gLight = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  bloomW = w / 2;
  if (bloomW < 1) {
    bloomW = 1;
  }
  bloomH = h / 2;
  if (bloomH < 1) {
    bloomH = 1;
  }
  bloomA = rhi->CreateRenderTarget(bloomW, bloomH, RHIFormat::RGBA16F);
  bloomB = rhi->CreateRenderTarget(bloomW, bloomH, RHIFormat::RGBA16F);
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
  ssrW = w;
  ssrH = h;
  ssrA = rhi->CreateRenderTarget(ssrW, ssrH, RHIFormat::RGBA8_UNORM);
  contactA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  filmA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  ssgiA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  ssgiB = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  fxaaA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gradeA = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  return gAlbedo != 0 && gNormalRough != 0 && gMetallic != 0 && gPosition != 0 && gLightmap != 0 && gEmissive != 0 && gF0 != 0 && gExtA != 0 && transSrc != 0 && gDepth != 0 && gViewport != 0 && gLight != 0 && bloomA != 0 && bloomB != 0 && fsrA != 0 && fsrOut != 0 && ssaoA != 0 && ssaoB != 0 && ssrA != 0 && contactA != 0 && filmA != 0 && ssgiA != 0 && ssgiB != 0 && fxaaA != 0 && gradeA != 0;
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
  if (gLightmap != 0) {
    rhi->DestroyRenderTarget(gLightmap);
    gLightmap = 0;
  }
  if (gEmissive != 0) {
    rhi->DestroyRenderTarget(gEmissive);
    gEmissive = 0;
  }
  if (gF0 != 0) {
    rhi->DestroyRenderTarget(gF0);
    gF0 = 0;
  }
  if (gExtA != 0) {
    rhi->DestroyRenderTarget(gExtA);
    gExtA = 0;
  }
  if (transSrc != 0) {
    rhi->DestroyRenderTarget(transSrc);
    transSrc = 0;
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
  if (ssrA != 0) {
    rhi->DestroyRenderTarget(ssrA);
    ssrA = 0;
  }
  if (contactA != 0) {
    rhi->DestroyRenderTarget(contactA);
    contactA = 0;
  }
  if (filmA != 0) {
    rhi->DestroyRenderTarget(filmA);
    filmA = 0;
  }
  if (ssgiA != 0) {
    rhi->DestroyRenderTarget(ssgiA);
    ssgiA = 0;
  }
  if (ssgiB != 0) {
    rhi->DestroyRenderTarget(ssgiB);
    ssgiB = 0;
  }
  if (fxaaA != 0) {
    rhi->DestroyRenderTarget(fxaaA);
    fxaaA = 0;
  }
  if (gradeA != 0) {
    rhi->DestroyRenderTarget(gradeA);
    gradeA = 0;
  }
}
}
