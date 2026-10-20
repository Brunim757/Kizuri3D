#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Window.h"
#include "Kizuri/Input.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/RHI.h"
#include "Kizuri/Camera.h"
#include "Kizuri/MeshLoader.h"
#include "Kizuri/DeferredRenderer.h"
#include <DirectXMath.h>
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <string>
namespace {
bool TryLoad(const char* p, Kizuri::StaticMesh& m) {
  return Kizuri::LoadStaticMeshFromGltf(p, m);
}
void BuildFallbackCube(Kizuri::StaticMesh& m) {
  float v[8][3] = {
    {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f},
    {0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{0.5f,0.5f,-0.5f}
  };
  float n[6][3] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
  int faces[6][4] = {{0,1,2,3},{4,5,6,7},{5,0,3,6},{1,4,7,2},{3,2,7,6},{5,4,1,0}};
  m.positions.clear();
  m.normals.clear();
  m.uvs.clear();
  m.indices.clear();
  float tuv[4][2] = {{0,0},{1,0},{1,1},{0,1}};
  for (int f = 0; f < 6; ++f) {
    uint32_t base = static_cast<uint32_t>(m.positions.size() / 3);
    for (int k = 0; k < 4; ++k) {
      m.positions.push_back(v[faces[f][k]][0]);
      m.positions.push_back(v[faces[f][k]][1]);
      m.positions.push_back(v[faces[f][k]][2]);
      m.normals.push_back(n[f][0]);
      m.normals.push_back(n[f][1]);
      m.normals.push_back(n[f][2]);
      m.uvs.push_back(tuv[k][0]);
      m.uvs.push_back(tuv[k][1]);
    }
    m.indices.push_back(base);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
    m.indices.push_back(base);
    m.indices.push_back(base + 2);
    m.indices.push_back(base + 3);
  }
}
}
int main() {
  std::printf("Kizuri Fase2Demo deferred PBR\n");
  Kizuri::Window win;
  if (!win.Create(L"Kizuri Fase2 - Deferred PBR", 1280, 720)) {
    std::printf("Window create failed\n");
    return 1;
  }
  Kizuri::RawInputPoll::Initialize(win.NativeHandle());
  Kizuri::RHIDesc desc;
  desc.windowHandle = win.NativeHandle();
  desc.width = win.Width();
  desc.height = win.Height();
  desc.vsync = true;
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::D3D11);
  if (rhi == nullptr || !rhi->Initialize(desc)) {
    std::printf("RHI DX11 init failed\n");
    if (rhi != nullptr) {
      const char* reason = rhi->LastError();
      if (reason != nullptr && reason[0] != '\0') {
        std::printf("Detalhe: %s\n", reason);
      }
    }
    return 1;
  }
  const char* shaderDirs[4] = { "Shaders", "../Shaders", "../../Shaders", "build/bin/Release/Shaders" };
  const char* meshPaths[4] = { "Assets/cube.gltf", "Samples/Assets/cube.gltf", "../Assets/cube.gltf", "build/bin/Release/Assets/cube.gltf" };
  Kizuri::DeferredRenderer renderer;
  bool rok = false;
  std::string usedShaders;
  for (int i = 0; i < 4 && !rok; ++i) {
    if (renderer.Initialize(rhi, win.Width(), win.Height(), shaderDirs[i])) {
      rok = true;
      usedShaders = shaderDirs[i];
    } else {
      renderer.Shutdown();
    }
  }
  if (!rok) {
    std::printf("Deferred init failed\n");
    return 1;
  }
  std::printf("Shaders: %s backend: %s\n", usedShaders.c_str(), rhi->BackendName());
  Kizuri::StaticMesh mesh;
  bool mok = false;
  for (int i = 0; i < 4 && !mok; ++i) {
    if (TryLoad(meshPaths[i], mesh)) {
      mok = true;
      std::printf("Mesh: %s verts=%llu idx=%llu\n", meshPaths[i], (unsigned long long)(mesh.positions.size() / 3), (unsigned long long)mesh.indices.size());
    }
  }
  if (!mok) {
    BuildFallbackCube(mesh);
    std::printf("Mesh fallback procedural cube\n");
  }
  if (!renderer.SetMesh(mesh.positions.data(), mesh.normals.data(), mesh.uvs.data(), mesh.positions.size() / 3, mesh.indices.data(), mesh.indices.size())) {
    std::printf("SetMesh failed\n");
    return 1;
  }
  Kizuri::DeferredMaterial mat;
  mat.albedo[0] = 0.75f;
  mat.albedo[1] = 0.22f;
  mat.albedo[2] = 0.12f;
  mat.roughness = 0.45f;
  mat.metallic = 0.05f;
  renderer.SetMaterial(mat);
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 1.2f, -3.5f);
  int lastX = 0;
  int lastY = 0;
  Kizuri::RawInputPoll::GetMousePosition(lastX, lastY);
  unsigned long long prev = GetTickCount64();
  unsigned long long start = prev;
  int frames = 0;
  while (win.PollEvents()) {
    Kizuri::RawInputPoll::Poll();
    if (Kizuri::RawInputPoll::IsKeyDown(VK_ESCAPE)) {
      break;
    }
    unsigned long long now = GetTickCount64();
    float dt = static_cast<float>(now - prev) / 1000.0f;
    prev = now;
    if (dt > 0.1f) {
      dt = 0.1f;
    }
    float t = static_cast<float>(now - start) / 1000.0f;
    int mx = 0;
    int my = 0;
    Kizuri::RawInputPoll::GetMousePosition(mx, my);
    int mdx = mx - lastX;
    int mdy = my - lastY;
    lastX = mx;
    lastY = my;
    bool look = Kizuri::RawInputPoll::IsMouseDown(VK_RBUTTON);
    cam.Update(
      dt,
      Kizuri::RawInputPoll::IsKeyDown(0x57),
      Kizuri::RawInputPoll::IsKeyDown(0x53),
      Kizuri::RawInputPoll::IsKeyDown(0x41),
      Kizuri::RawInputPoll::IsKeyDown(0x44),
      Kizuri::RawInputPoll::IsKeyDown(0x45),
      Kizuri::RawInputPoll::IsKeyDown(0x51),
      look ? static_cast<float>(mdx) : 0.0f,
      look ? static_cast<float>(mdy) : 0.0f);
    Kizuri::DeferredLight light;
    light.direction[0] = sinf(t * 0.4f);
    light.direction[1] = -1.0f;
    light.direction[2] = cosf(t * 0.4f) * 0.6f;
    light.color[0] = 1.0f;
    light.color[1] = 0.96f;
    light.color[2] = 0.9f;
    light.intensity = 3.0f;
    renderer.SetLight(light);
    DirectX::XMMATRIX view = cam.View();
    float aspect = static_cast<float>(win.Width()) / static_cast<float>(win.Height());
    DirectX::XMMATRIX proj = cam.Projection(aspect);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, view);
    DirectX::XMStoreFloat4x4(&pf, proj);
    float cpx = 0.0f;
    float cpy = 0.0f;
    float cpz = 0.0f;
    cam.GetPosition(cpx, cpy, cpz);
    float cpos[3] = { cpx, cpy, cpz };
    renderer.Render(&vf.m[0][0], &pf.m[0][0], cpos);
    rhi->Present(true);
    ++frames;
    if (frames == 60) {
      std::printf("60 frames ok WASD move QE up-down RButton look ESC quit\n");
    }
  }
  std::printf("frames=%d\n", frames);
  renderer.Shutdown();
  Kizuri::DestroyRHI(rhi);
  Kizuri::RawInputPoll::Shutdown();
  win.Destroy();
  return 0;
}
