#include "Kizuri/Core.h"
#include "Kizuri/Render.h"
#include "Kizuri/Physics.h"
#include "Kizuri/Animation.h"
#include "Kizuri/Audio.h"
#include "Kizuri/Scripting.h"
#include "Kizuri/Allocator.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/Origin.h"
#include "Kizuri/RHI.h"
#include "Kizuri/Camera.h"
#include "Kizuri/MeshLoader.h"
#include "Kizuri/PBR.h"
#include "Kizuri/DebugDraw.h"
#include "Kizuri/Shadows.h"
#include "Kizuri/DeferredRenderer.h"
#include "Kizuri/Scene.h"
#include "Kizuri/MultiSelection.h"
#include "Kizuri/Log.h"
#include "Kizuri/EditQueue.h"
#include "Kizuri/Picking.h"
#include "Kizuri/Reflection.h"
#include "Kizuri/SceneSerializer.h"
#include "Kizuri/Undo.h"
#include "Kizuri/Autosave.h"
#include "Kizuri/Notifications.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/MeshCodec.h"
#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include <DirectXMath.h>
#include <filesystem>
#include <TaskScheduler.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <atomic>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3dcompiler.h>
#undef small
#undef far
#undef near
#endif
namespace {
int Check(const char* name, bool ok) {
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  std::fflush(stdout);
  return ok ? 0 : 1;
}
struct AddTask : public enki::ITaskSet {
  std::atomic<int>* acc;
  int value;
  AddTask(std::atomic<int>* a, int v) : acc(a), value(v) { m_SetSize = 1; }
  void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
    acc->fetch_add(value);
  }
};
bool TestPool() {
  Kizuri::PoolAllocator pool(64, 128, 16);
  if (!pool.Initialize()) {
    return false;
  }
  void* ptrs[128];
  for (int i = 0; i < 128; ++i) {
    ptrs[i] = pool.Allocate();
    if (ptrs[i] == nullptr) {
      return false;
    }
    std::memset(ptrs[i], 0xAB, 64);
  }
  if (pool.Allocate() != nullptr) {
    return false;
  }
  for (int i = 0; i < 128; ++i) {
    pool.Free(ptrs[i]);
  }
  if (pool.FreeCount() != pool.Capacity()) {
    return false;
  }
  void* a = pool.Allocate();
  void* b = pool.Allocate();
  bool ok = (a != nullptr && b != nullptr && a != b);
  pool.Free(a);
  pool.Free(b);
  pool.Shutdown();
  return ok;
}
bool TestArena() {
  Kizuri::ArenaAllocator arena(4096);
  if (!arena.Initialize()) {
    return false;
  }
  void* a = arena.Allocate(256, 16);
  void* b = arena.Allocate(512, 16);
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if ((reinterpret_cast<uintptr_t>(a) % 16) != 0) {
    return false;
  }
  if ((reinterpret_cast<uintptr_t>(b) % 16) != 0) {
    return false;
  }
  if (arena.Used() == 0) {
    return false;
  }
  arena.Reset();
  if (arena.Used() != 0) {
    return false;
  }
  void* c = arena.Allocate(1024, 8);
  bool ok = (c != nullptr);
  arena.Shutdown();
  return ok;
}
bool TestJobSystem() {
  Kizuri::JobSystem js;
  if (!js.Initialize()) {
    return false;
  }
  if (js.GetThreadCount() == 0) {
    return false;
  }
  std::atomic<int> acc(0);
  AddTask t1(&acc, 10);
  AddTask t2(&acc, 20);
  AddTask t3(&acc, 30);
  AddTask t4(&acc, 40);
  js.AddTask(&t1);
  js.AddTask(&t2);
  js.AddTask(&t3);
  js.AddTask(&t4);
  js.WaitForTask(&t1);
  js.WaitForTask(&t2);
  js.WaitForTask(&t3);
  js.WaitForTask(&t4);
  bool ok = (acc.load() == 100);
  js.Shutdown();
  return ok;
}
bool TestOrigin() {
  Kizuri::OriginRebaser o;
  double ox;
  double oy;
  double oz;
  o.GetOrigin(ox, oy, oz);
  if (ox != 0.0 || oy != 0.0 || oz != 0.0) {
    return false;
  }
  float lx;
  float ly;
  float lz;
  o.WorldToLocal(100000.0, 0.0, 0.0, lx, ly, lz);
  if (lx < 99999.0f || lx > 100001.0f) {
    return false;
  }
  bool rebased = o.RebaseIfNeeded(100000.0, 0.0, 0.0, 5000.0);
  if (!rebased) {
    return false;
  }
  o.GetOrigin(ox, oy, oz);
  if (ox != 100000.0) {
    return false;
  }
  o.WorldToLocal(100010.0, 5.0, -3.0, lx, ly, lz);
  if (lx < 9.9f || lx > 10.1f) {
    return false;
  }
  double wx;
  double wy;
  double wz;
  o.LocalToWorld(lx, ly, lz, wx, wy, wz);
  double dx = wx - 100010.0;
  double dy = wy - 5.0;
  double dz = wz + 3.0;
  if (dx < -0.01 || dx > 0.01) {
    return false;
  }
  if (dy < -0.01 || dy > 0.01) {
    return false;
  }
  if (dz < -0.01 || dz > 0.01) {
    return false;
  }
  bool notNeeded = o.RebaseIfNeeded(100010.0, 5.0, -3.0, 5000.0);
  if (notNeeded) {
    return false;
  }
  return true;
}
bool TestRHINull() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 1280;
  desc.height = 720;
  desc.vsync = true;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (std::strcmp(rhi->BackendName(), "Null") != 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->Clear(0.1f, 0.2f, 0.3f, 1.0f);
  if (!rhi->Resize(800, 600)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->Width() != 800 || rhi->Height() != 600) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->Present(true);
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
}
bool TestStateCache() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 640;
  desc.height = 480;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::RHIRasterizer rs;
  rs.cull = Kizuri::RHICull::Back;
  rs.fill = Kizuri::RHIFill::Solid;
  rs.frontCCW = false;
  rs.slopeBias = 0.0f;
  rhi->SetRasterizerState(rs);
  rhi->SetRasterizerState(rs);
  rhi->SetRasterizerState(rs);
  Kizuri::RHIViewport vp;
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.w = 640.0f;
  vp.h = 480.0f;
  vp.minD = 0.0f;
  vp.maxD = 1.0f;
  rhi->SetViewport(vp);
  rhi->SetViewport(vp);
  uint64_t total = 0;
  uint64_t discarded = 0;
  rhi->GetCacheStats(total, discarded);
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return total >= 5 && discarded >= 3;
}
bool TestCamera() {
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMVECTOR origin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
  DirectX::XMVECTOR vs = DirectX::XMVector3TransformCoord(origin, v);
  DirectX::XMFLOAT3 o;
  DirectX::XMStoreFloat3(&o, vs);
  if (o.z < 4.9f || o.z > 5.1f) {
    return false;
  }
  DirectX::XMMATRIX p = cam.Projection(16.0f / 9.0f);
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&pf, p);
  if (pf.m[0][0] <= 0.0f || pf.m[1][1] <= 0.0f) {
    return false;
  }
  return true;
}
bool TestMeshLoader() {
  const char* paths[3] = { "Samples/Assets/cube.gltf", "Assets/cube.gltf", "build/bin/Release/Assets/cube.gltf" };
  for (int i = 0; i < 3; ++i) {
    Kizuri::StaticMesh m;
    if (Kizuri::LoadStaticMeshFromGltf(paths[i], m)) {
      if (m.positions.size() / 3 != 24) {
        return false;
      }
      if (m.indices.size() != 36) {
        return false;
      }
      if (m.normals.size() != m.positions.size()) {
        return false;
      }
      return true;
    }
  }
  return false;
}
bool TestPBR() {
  float albedo[3] = { 0.8f, 0.2f, 0.15f };
  float N[3] = { 0.0f, 1.0f, 0.0f };
  float V[3] = { 0.3f, 0.9f, 0.2f };
  float L[3] = { 0.4f, 0.9f, 0.1f };
  float light[3] = { 3.0f, 3.0f, 3.0f };
  float out[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::PBR_Directional(albedo, 0.5f, 0.0f, N, V, L, light, out);
  if (!(out[0] > 0.0f && out[1] > 0.0f && out[2] > 0.0f)) {
    return false;
  }
  float out2[3] = { 0.0f, 0.0f, 0.0f };
  float Ld[3] = { 0.0f, -1.0f, 0.0f };
  Kizuri::PBR_Directional(albedo, 0.5f, 0.0f, N, V, Ld, light, out2);
  if (!(out2[0] == 0.0f && out2[1] == 0.0f && out2[2] == 0.0f)) {
    return false;
  }
  return true;
}
bool TestShaderFiles() {
  const char* dirs[3] = { "Shaders", "Samples/../Shaders", "build/bin/Release/Shaders" };
  const char* files[5] = { "GeometryVS.hlsl", "GeometryPS.hlsl", "LightingVS.hlsl", "LightingPS.hlsl", "DepthVS.hlsl" };
  for (int d = 0; d < 3; ++d) {
    bool allOk = true;
    for (int f = 0; f < 5; ++f) {
      char path[512];
      std::snprintf(path, sizeof(path), "%s/%s", dirs[d], files[f]);
      FILE* fp = std::fopen(path, "rb");
      if (fp == nullptr) {
        allOk = false;
        break;
      }
      std::fseek(fp, 0, SEEK_END);
      long sz = std::ftell(fp);
      std::fclose(fp);
      if (sz < 100) {
        allOk = false;
        break;
      }
    }
    if (allOk) {
      return true;
    }
  }
  return false;
}
bool TestDeferredNull() {  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 320;
  desc.height = 200;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::DeferredRenderer renderer;
  const char* shaderDirs[3] = { "Shaders", "Samples/../Shaders", "." };
  bool rok = false;
  for (int i = 0; i < 3 && !rok; ++i) {
    if (renderer.Initialize(rhi, 320, 200, shaderDirs[i])) {
      rok = true;
    } else {
      renderer.Shutdown();
    }
  }
  if (!rok) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float tri[9] = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f };
  float nrm[9] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
  float uv[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f };
  uint32_t idx[3] = { 0, 1, 2 };
  if (!renderer.SetMesh(tri, nrm, uv, 3, idx, 3)) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float view[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,5,1 };
  float proj[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
  float cam[3] = { 0, 0, -5 };
  renderer.Render(view, proj, cam);
  if (!renderer.IsReady()) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (!renderer.Resize(160, 100)) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  renderer.Render(view, proj, cam);
  renderer.RenderToTexture(view, proj, cam);
  void* srv = renderer.GetViewportTexture();
  if (srv != nullptr) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
  renderer.BeginObjects(view, proj);
  renderer.DrawObject(ident);
  renderer.DrawObject(ident);
  Kizuri::RHITexture t0 = rhi->CreateTexture2D(4, 4, 1, Kizuri::RHITextureFormat::RGBA8_UNORM);
  if (t0 == 0) {
    renderer.EndObjectsToTexture(cam);
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float verts[12] = { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 };
  uint32_t idxs[3] = { 0, 1, 2 };
  Kizuri::RHIBuffer tvb = rhi->CreateBuffer(sizeof(verts), 48, false, verts);
  Kizuri::RHIBuffer tib = rhi->CreateBuffer(sizeof(idxs), 4, true, idxs);
  if (tvb == 0 || tib == 0) {
    renderer.EndObjectsToTexture(cam);
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::DeferredMaterial tmat;
  tmat.albedo[0] = 1.0f;
  tmat.albedo[1] = 1.0f;
  tmat.albedo[2] = 1.0f;
  tmat.roughness = 0.5f;
  tmat.metallic = 0.0f;
  tmat.albedoTex = t0;
  tmat.normalTex = t0;
  tmat.ormTex = 0;
  tmat.texFlags = 3;
  renderer.DrawObjectEx(ident, tvb, tib, 0, 3, &tmat);
  renderer.DrawObjectEx(ident, tvb, tib, 0, 0, &tmat);
  renderer.DrawObjectEx(ident, 0, tib, 0, 3, &tmat);
  renderer.DrawObjectEx(ident, tvb, tib, 0, 3, nullptr);
  rhi->DestroyBuffer(tvb);
  rhi->DestroyBuffer(tib);
  rhi->DestroyTexture(t0);
  renderer.EndObjectsToTexture(cam);
  renderer.BeginObjects(view, proj);
  renderer.EndObjectsToBackbuffer(cam);
  renderer.Shutdown();
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
}
bool TestSceneCrud() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("Alpha");
  Kizuri::EntityId b = scene.CreateEntity("Beta");
  if (!a.IsValid() || !b.IsValid() || a == b) {
    return false;
  }
  if (scene.Count() != 2) {
    return false;
  }
  if (!scene.RenameEntity(a, "Gamma")) {
    return false;
  }
  const Kizuri::Entity* e = scene.Get(a);
  if (e == nullptr || e->name != "Gamma") {
    return false;
  }
  if (e->transform.position[0] != 0.0f || e->transform.scale[0] != 1.0f) {
    return false;
  }
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 3.0f;
  if (!scene.SetTransform(a, t)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 3.0f) {
    return false;
  }
  if (!scene.IsDirty()) {
    return false;
  }
  scene.ClearDirty();
  if (scene.IsDirty()) {
    return false;
  }
  if (!scene.DeleteEntity(a)) {
    return false;
  }
  if (scene.Has(a) || scene.Count() != 1) {
    return false;
  }
  if (scene.Get(a) != nullptr) {
    return false;
  }
  Kizuri::EntityId c = scene.CreateEntity("Delta");
  if (!c.IsValid() || scene.Count() != 2) {
    return false;
  }
  if (scene.DeleteEntity(Kizuri::EntityId::Invalid())) {
    return false;
  }
  return true;
}
bool TestSceneDuplicate() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("Base");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[1] = 2.0f;
  t.scale[0] = 3.0f;
  scene.SetTransform(a, t);
  Kizuri::EntityId copy = scene.DuplicateEntity(a);
  if (!copy.IsValid() || copy == a) {
    return false;
  }
  const Kizuri::Entity* e = scene.Get(copy);
  if (e == nullptr || e->name != "Base Copy") {
    return false;
  }
  if (e->transform.position[1] != 2.0f || e->transform.scale[0] != 3.0f) {
    return false;
  }
  if (scene.Count() != 2) {
    return false;
  }
  if (scene.DuplicateEntity(Kizuri::EntityId::Invalid()).IsValid()) {
    return false;
  }
  return true;
}
bool TestSceneParent() {
  Kizuri::Scene scene;
  Kizuri::EntityId root = scene.CreateEntity("Root");
  Kizuri::EntityId child = scene.CreateEntity("Child");
  Kizuri::EntityId grand = scene.CreateEntity("Grand");
  if (!scene.SetParent(child, root)) {
    return false;
  }
  if (!scene.SetParent(grand, child)) {
    return false;
  }
  if (scene.SetParent(root, grand)) {
    return false;
  }
  if (scene.SetParent(root, root)) {
    return false;
  }
  const Kizuri::Entity* r = scene.Get(root);
  if (r == nullptr || r->children.size() != 1 || !(r->children[0] == child)) {
    return false;
  }
  if (!scene.SetParent(child, Kizuri::EntityId::Invalid())) {
    return false;
  }
  if (scene.Get(root)->children.size() != 0) {
    return false;
  }
  if (!scene.SetParent(child, root)) {
    return false;
  }
  if (!scene.DeleteEntity(root)) {
    return false;
  }
  if (scene.Count() != 0 || scene.Has(child) || scene.Has(grand)) {
    return false;
  }
  return true;
}
bool TestSelectionSet() {
  Kizuri::Scene scene;
  Kizuri::SelectionSet sel;
  if (sel.HasSelection() || sel.Count() != 0) {
    return false;
  }
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::EntityId c = scene.CreateEntity("C");
  sel.Select(a);
  if (!sel.HasSelection() || !sel.Contains(a) || sel.Contains(b) || sel.Count() != 1) {
    return false;
  }
  if (!(sel.Get() == a) || !(sel.Primary() == a) || !(sel.At(0) == a)) {
    return false;
  }
  sel.Add(b);
  sel.Add(b);
  if (sel.Count() != 2 || !sel.Contains(b)) {
    return false;
  }
  sel.Toggle(b);
  if (sel.Contains(b) || sel.Count() != 1) {
    return false;
  }
  sel.Toggle(c);
  if (!sel.Contains(c) || sel.Count() != 2) {
    return false;
  }
  sel.Remove(a);
  if (sel.Contains(a) || sel.Count() != 1 || !(sel.Primary() == c)) {
    return false;
  }
  sel.Select(b);
  scene.DeleteEntity(b);
  sel.OnEntityDeleted(b);
  if (sel.HasSelection()) {
    return false;
  }
  sel.Add(a);
  sel.Add(c);
  scene.DeleteEntity(a);
  sel.OnEntityDeleted(a);
  if (sel.Count() != 1 || !sel.Contains(c)) {
    return false;
  }
  sel.Clear();
  if (sel.HasSelection() || !sel.IsEmpty()) {
    return false;
  }
  if (sel.At(99).IsValid()) {
    return false;
  }
  return true;
}
bool TestScreenRect() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMStoreFloat4x4(&pf, p);
  float x0;
  float y0;
  float x1;
  float y1;
  if (!Kizuri::EntityScreenRect(scene, a, &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  if (x0 > 400.0f || x1 < 400.0f || y0 > 300.0f || y1 < 300.0f) {
    return false;
  }
  if (x1 - x0 <= 1.0f || y1 - y0 <= 1.0f) {
    return false;
  }
  return true;
}
bool TestScreenRectBehind() {
  Kizuri::Scene scene;
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::Entity* e = scene.Get(b);
  e->transform.position[0] = 0.0f;
  e->transform.position[1] = 0.0f;
  e->transform.position[2] = -50.0f;
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMStoreFloat4x4(&pf, p);
  float x0;
  float y0;
  float x1;
  float y1;
  if (Kizuri::EntityScreenRect(scene, b, &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  if (Kizuri::EntityScreenRect(scene, Kizuri::EntityId::Invalid(), &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  return true;
}
bool TestRectOverlap() {
  if (!Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 5.0f, 5.0f, 15.0f, 15.0f)) {
    return false;
  }
  if (Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 20.0f, 20.0f, 30.0f, 30.0f)) {
    return false;
  }
  if (!Kizuri::RectsOverlap(10.0f, 10.0f, 0.0f, 0.0f, 5.0f, 5.0f, 15.0f, 15.0f)) {
    return false;
  }
  if (!Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 0.0f, 20.0f, 10.0f)) {
    return false;
  }
  return true;
}
bool TestLog() {
  Kizuri::LogStore log;
  log.Add(Kizuri::LogLevel::Info, "hello");
  log.Add(Kizuri::LogLevel::Error, "boom");
  log.Add(Kizuri::LogLevel::Warning, "careful");
  log.Add(Kizuri::LogLevel::Success, "done");
  if (log.Count() != 4) {
    return false;
  }
  if (log.CountLevel(Kizuri::LogLevel::Error) != 1) {
    return false;
  }
  if (log.At(0).text != "hello" || log.At(3).level != Kizuri::LogLevel::Success) {
    return false;
  }
  if (log.At(0).seq >= log.At(3).seq) {
    return false;
  }
  log.Clear();
  return log.Count() == 0;
}
bool TestEditQueue() {
  Kizuri::Scene scene;
  Kizuri::EditQueue queue;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::Transform t1;
  Kizuri::MakeIdentityTransform(t1);
  t1.position[0] = 1.0f;
  Kizuri::Transform t2;
  Kizuri::MakeIdentityTransform(t2);
  t2.position[0] = 2.0f;
  queue.PushTransform(a, t1);
  queue.PushTransform(a, t2);
  queue.PushTransform(Kizuri::EntityId::Invalid(), t1);
  if (queue.Pending() != 2) {
    return false;
  }
  size_t applied = queue.ApplyAll(scene, undo);
  if (applied != 1 || queue.Pending() != 0) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 2.0f) {
    return false;
  }
  if (!undo.CanUndo()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  Kizuri::Transform t3;
  Kizuri::MakeIdentityTransform(t3);
  t3.position[2] = 9.0f;
  queue.PushTransform(a, t3);
  queue.Clear();
  if (queue.Pending() != 0) {
    return false;
  }
  return scene.Get(a)->transform.position[2] == 0.0f;
}
bool TestPicking() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::Entity* e = scene.Get(a);
  e->transform.position[0] = 0.0f;
  e->transform.position[1] = 0.0f;
  e->transform.position[2] = 0.0f;
  float view[16];
  float proj[16];
  {
    Kizuri::FreeCamera cam;
    cam.SetPosition(0.0f, 0.0f, -5.0f);
    cam.SetYawPitch(0.0f, 0.0f);
    DirectX::XMMATRIX v = cam.View();
    DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, v);
    DirectX::XMStoreFloat4x4(&pf, p);
    std::memcpy(view, &vf.m[0][0], sizeof(view));
    std::memcpy(proj, &pf.m[0][0], sizeof(proj));
  }
  float origin[3];
  float dir[3];
  Kizuri::ScreenPointRay(400.0f, 300.0f, 800.0f, 600.0f, view, proj, origin, dir);
  Kizuri::EntityId hit = Kizuri::PickFirst(scene, origin, dir);
  if (!(hit == a)) {
    return false;
  }
  float farOrigin[3] = { 50.0f, 50.0f, 50.0f };
  float farDir[3] = { 0.0f, 1.0f, 0.0f };
  if (Kizuri::PickFirst(scene, farOrigin, farDir).IsValid()) {
    return false;
  }
  float m[16];
  Kizuri::ComposeMatrix(e->transform, m);
  float t = 0.0f;
  float ro[3] = { 0.0f, 0.0f, 5.0f };
  float rd[3] = { 0.0f, 0.0f, -1.0f };
  if (!Kizuri::RayVsUnitCube(ro, rd, m, t)) {
    return false;
  }
  if (t < 4.4f || t > 4.6f) {
    return false;
  }
  float rdUp[3] = { 0.0f, 1.0f, 0.0f };
  if (Kizuri::RayVsUnitCube(ro, rdUp, m, t)) {
    return false;
  }
  return true;
}
bool TestSceneRoundTrip() {
  Kizuri::Scene src;
  Kizuri::EntityId root = src.CreateEntity("Root Node");
  Kizuri::EntityId child = src.CreateEntity("Child \"Quoted\" \\ Test");
  Kizuri::EntityId lone = src.CreateEntity("Lone");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 1.5f;
  t.position[1] = -2.25f;
  t.position[2] = 100.125f;
  t.rotation[0] = 30.0f;
  t.rotation[1] = 45.5f;
  t.rotation[2] = -10.0f;
  t.scale[0] = 2.0f;
  t.scale[1] = 0.5f;
  t.scale[2] = 1.0f;
  src.SetTransform(root, t);
  Kizuri::Transform tc;
  Kizuri::MakeIdentityTransform(tc);
  tc.position[2] = 7.0f;
  src.SetTransform(child, tc);
  src.SetParent(child, root);
  const char* path = "test_scene_tmp.kzscene";
  std::remove(path);
  if (!Kizuri::SaveSceneToFile(src, path)) {
    return false;
  }
  Kizuri::Scene dst;
  if (!Kizuri::LoadSceneFromFile(dst, path, nullptr)) {
    std::remove(path);
    return false;
  }
  std::remove(path);
  if (dst.Count() != 3 || dst.IsDirty()) {
    return false;
  }
  const Kizuri::Entity* dr = nullptr;
  const Kizuri::Entity* dc = nullptr;
  const Kizuri::Entity* dl = nullptr;
  std::vector<Kizuri::EntityId> all = dst.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* e = dst.Get(all[i]);
    if (e->name == "Root Node") {
      dr = e;
    } else if (e->name == "Child \"Quoted\" \\ Test") {
      dc = e;
    } else if (e->name == "Lone") {
      dl = e;
    }
  }
  if (dr == nullptr || dc == nullptr || dl == nullptr) {
    return false;
  }
  if (dr->transform.position[0] != 1.5f || dr->transform.position[1] != -2.25f || dr->transform.position[2] != 100.125f) {
    return false;
  }
  if (dr->transform.rotation[1] != 45.5f || dr->transform.scale[0] != 2.0f) {
    return false;
  }
  if (dc->transform.position[2] != 7.0f) {
    return false;
  }
  if (!dc->parent.IsValid()) {
    return false;
  }
  const Kizuri::Entity* dp = dst.Get(dc->parent);
  if (dp == nullptr || dp->name != "Root Node") {
    return false;
  }
  if (dl->parent.IsValid()) {
    return false;
  }
  if (Kizuri::TypeRegistry::Instance().Find("Transform") == nullptr) {
    return false;
  }
  (void)lone;
  return true;
}
bool TestSceneInvalid() {
  Kizuri::Scene scene;
  if (Kizuri::LoadSceneFromFile(scene, "no_such_file_xyz.kzscene", nullptr)) {
    return false;
  }
  const char* bad = "test_scene_bad_tmp.kzscene";
  FILE* fp = std::fopen(bad, "wb");
  if (fp == nullptr) {
    return false;
  }
  std::fputs("NOT A SCENE\nGARBAGE {{{{\n", fp);
  std::fclose(fp);
  bool ok = !Kizuri::LoadSceneFromFile(scene, bad, nullptr);
  std::remove(bad);
  if (!ok) {
    return false;
  }
  const char* bad2 = "test_scene_bad2_tmp.kzscene";
  fp = std::fopen(bad2, "wb");
  if (fp == nullptr) {
    return false;
  }
  std::fputs("KZSCENE 1\nENTITY \"Broken\" notanumber\n", fp);
  std::fclose(fp);
  ok = !Kizuri::LoadSceneFromFile(scene, bad2, nullptr);
  std::remove(bad2);
  return ok;
}
bool TestUndoCreate() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 5.0f;
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::CreateEntityCmd("Undo1", t, Kizuri::EntityId::Invalid()));
  if (!undo.Execute(std::move(cmd), scene) || scene.Count() != 1) {
    return false;
  }
  if (!undo.CanUndo() || undo.UndoDepth() != 1) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Count() != 0) {
    return false;
  }
  if (!undo.CanRedo()) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Count() != 1) {
    return false;
  }
  std::vector<Kizuri::EntityId> all = scene.All();
  const Kizuri::Entity* e = scene.Get(all[0]);
  return e != nullptr && e->name == "Undo1" && e->transform.position[0] == 5.0f;
}
bool TestUndoDelete() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("Keep");
  Kizuri::EntityId b = scene.CreateEntity("Gone");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[1] = 3.0f;
  scene.SetTransform(b, t);
  scene.SetParent(b, a);
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::DeleteEntityCmd(b));
  if (!undo.Execute(std::move(cmd), scene) || scene.Count() != 1) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Count() != 2) {
    return false;
  }
  const Kizuri::Entity* rb = nullptr;
  std::vector<Kizuri::EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* e = scene.Get(all[i]);
    if (e->name == "Gone") {
      rb = e;
    }
  }
  if (rb == nullptr || rb->transform.position[1] != 3.0f) {
    return false;
  }
  if (!rb->parent.IsValid()) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Count() != 1) {
    return false;
  }
  return true;
}
bool TestUndoEdit() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("Mover");
  const Kizuri::Entity* e0 = scene.Get(a);
  Kizuri::Transform before = e0->transform;
  Kizuri::Transform after = before;
  after.position[0] = 9.0f;
  after.rotation[1] = 45.0f;
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::EditTransformCmd(a, before, after));
  if (!undo.Execute(std::move(cmd), scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 9.0f) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->transform.rotation[1] != 45.0f) {
    return false;
  }
  return true;
}
bool TestUndoRenameParent() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  std::unique_ptr<Kizuri::Command> rcmd(new Kizuri::RenameCmd(a, "A", "A2"));
  if (!undo.Execute(std::move(rcmd), scene) || scene.Get(a)->name != "A2") {
    return false;
  }
  std::unique_ptr<Kizuri::Command> pcmd(new Kizuri::SetParentCmd(b, Kizuri::EntityId::Invalid(), a));
  if (!undo.Execute(std::move(pcmd), scene)) {
    return false;
  }
  if (!scene.Get(b)->parent.IsValid()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(b)->parent.IsValid()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->name != "A") {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->name != "A2") {
    return false;
  }
  if (!undo.Redo(scene) || !scene.Get(b)->parent.IsValid()) {
    return false;
  }
  return true;
}
bool TestUndoStack() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  if (undo.CanUndo() || undo.CanRedo()) {
    return false;
  }
  if (undo.Undo(scene) || undo.Redo(scene)) {
    return false;
  }
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  std::unique_ptr<Kizuri::Command> c1(new Kizuri::CreateEntityCmd("One", t, Kizuri::EntityId::Invalid()));
  std::unique_ptr<Kizuri::Command> c2(new Kizuri::CreateEntityCmd("Two", t, Kizuri::EntityId::Invalid()));
  undo.Execute(std::move(c1), scene);
  undo.Execute(std::move(c2), scene);
  if (scene.Count() != 2 || undo.UndoDepth() != 2) {
    return false;
  }
  undo.Undo(scene);
  if (scene.Count() != 1 || undo.RedoDepth() != 1) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> c3(new Kizuri::CreateEntityCmd("Three", t, Kizuri::EntityId::Invalid()));
  undo.Execute(std::move(c3), scene);
  if (undo.CanRedo() || scene.Count() != 2) {
    return false;
  }
  undo.Clear();
  if (undo.CanUndo() || undo.CanRedo()) {
    return false;
  }
  return true;
}
bool TestUndoMultiMove() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::EntityId c = scene.CreateEntity("C");
  Kizuri::Transform ba = scene.Get(a)->transform;
  Kizuri::Transform bb = scene.Get(b)->transform;
  Kizuri::Transform bc = scene.Get(c)->transform;
  Kizuri::Transform na = ba;
  Kizuri::Transform nb = bb;
  Kizuri::Transform nc = bc;
  na.position[0] += 10.0f;
  nb.position[1] += 10.0f;
  nc.position[2] += 10.0f;
  na.rotation[0] += 15.0f;
  nb.scale[0] *= 2.0f;
  scene.SetTransform(a, na);
  scene.SetTransform(b, nb);
  scene.SetTransform(c, nc);
  Kizuri::MultiEditTransformCmd* multi = new Kizuri::MultiEditTransformCmd();
  multi->Add(a, ba, na);
  multi->Add(b, bb, nb);
  multi->Add(c, bc, nc);
  if (multi->Empty()) {
    delete multi;
    return false;
  }
  std::unique_ptr<Kizuri::Command> cmd(multi);
  undo.Commit(std::move(cmd));
  if (!undo.CanUndo()) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 10.0f || scene.Get(b)->transform.position[1] != 10.0f || scene.Get(c)->transform.position[2] != 10.0f) {
    return false;
  }
  if (!undo.Undo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 0.0f || scene.Get(b)->transform.position[1] != 0.0f || scene.Get(c)->transform.position[2] != 0.0f) {
    return false;
  }
  if (scene.Get(a)->transform.rotation[0] != 0.0f || scene.Get(b)->transform.scale[0] != 1.0f) {
    return false;
  }
  if (!undo.Redo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 10.0f || scene.Get(a)->transform.rotation[0] != 15.0f || scene.Get(b)->transform.scale[0] != 2.0f) {
    return false;
  }
  return true;
}
bool TestUndoMultiPartial() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::Transform ba = scene.Get(a)->transform;
  Kizuri::Transform bb = scene.Get(b)->transform;
  Kizuri::Transform na = ba;
  Kizuri::Transform nb = bb;
  na.position[0] = 4.0f;
  nb.position[0] = 8.0f;
  scene.SetTransform(a, na);
  scene.SetTransform(b, nb);
  Kizuri::MultiEditTransformCmd* multi = new Kizuri::MultiEditTransformCmd();
  multi->Add(a, ba, na);
  multi->Add(b, bb, nb);
  std::unique_ptr<Kizuri::Command> cmd(multi);
  undo.Commit(std::move(cmd));
  scene.DeleteEntity(b);
  if (!undo.Undo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  if (scene.Has(b)) {
    return false;
  }
  return true;
}
bool TestAutosavePaths() {
  std::string rec = Kizuri::RecoveryPathFor("C:/proj/scene.kzscene", "T");
  if (rec != "C:/proj/scene.kzscene.autosave.kzscene") {
    return false;
  }
  std::string u = Kizuri::RecoveryPathFor("", "T");
  std::filesystem::path p(u);
  if (p.filename().string() != "KizuriUntitled.autosave.kzscene") {
    return false;
  }
  if (p.parent_path().string() != "T") {
    return false;
  }
  return true;
}
bool TestAutosaveUpdate() {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "kzautosave_test";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::string rec = (dir / "r.kzscene").string();
  std::filesystem::remove(rec, ec);
  Kizuri::Scene scene;
  scene.CreateEntity("Auto");
  Kizuri::AutosaveManager m;
  if (m.Interval() != 60.0) {
    return false;
  }
  m.SetInterval(0.05);
  if (!m.Update(0.06, scene, rec)) {
    return false;
  }
  Kizuri::Scene back;
  if (!Kizuri::LoadSceneFromFile(back, rec, nullptr) || back.Count() != 1) {
    std::filesystem::remove(rec, ec);
    return false;
  }
  scene.ClearDirty();
  std::filesystem::remove(rec, ec);
  if (m.Update(5.0, scene, rec)) {
    return false;
  }
  if (std::filesystem::exists(rec, ec)) {
    std::filesystem::remove(rec, ec);
    return false;
  }
  m.SetInterval(0.0);
  scene.CreateEntity("Dirty2");
  if (m.Update(5.0, scene, rec)) {
    return false;
  }
  if (std::filesystem::exists(rec, ec)) {
    std::filesystem::remove(rec, ec);
    return false;
  }
  std::filesystem::remove_all(dir, ec);
  return true;
}
bool TestAutosaveOffer() {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "kzoffer_test";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::string main = (dir / "m.kzscene").string();
  std::string rec = (dir / "r.kzscene").string();
  {
    FILE* fp = std::fopen(main.c_str(), "wb");
    std::fputs("x", fp);
    std::fclose(fp);
    fp = std::fopen(rec.c_str(), "wb");
    std::fputs("y", fp);
    std::fclose(fp);
  }
  auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(main, now - std::chrono::seconds(10), ec);
  std::filesystem::last_write_time(rec, now, ec);
  if (!Kizuri::ShouldOfferRecovery(main, rec)) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  std::filesystem::last_write_time(main, now, ec);
  std::filesystem::last_write_time(rec, now - std::chrono::seconds(10), ec);
  if (Kizuri::ShouldOfferRecovery(main, rec)) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  if (Kizuri::ShouldOfferRecovery(main, (dir / "missing.kzscene").string())) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  std::filesystem::remove(main, ec);
  if (!Kizuri::ShouldOfferRecovery(main, rec)) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  if (!Kizuri::DeleteRecoveryFile(rec)) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  if (Kizuri::DeleteRecoveryFile(rec)) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  Kizuri::Scene s;
  s.CreateEntity("M");
  s.ClearDirty();
  if (s.IsDirty()) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  s.MarkDirty();
  if (!s.IsDirty()) {
    std::filesystem::remove_all(dir, ec);
    return false;
  }
  std::filesystem::remove_all(dir, ec);
  return true;
}
bool TestNotifications() {
  Kizuri::NotificationCenter n;
  if (n.Lifetime() != 4.0 || n.ActiveCount() != 0 || n.HistoryCount() != 0) {
    return false;
  }
  n.Notify(Kizuri::LogLevel::Info, "a");
  n.Notify(Kizuri::LogLevel::Error, "b");
  if (n.ActiveCount() != 2 || n.HistoryCount() != 2) {
    return false;
  }
  std::vector<Kizuri::Notification> active = n.Active();
  if (active[0].text != "a" || active[1].level != Kizuri::LogLevel::Error) {
    return false;
  }
  if (active[0].seq >= active[1].seq) {
    return false;
  }
  n.Update(5.0);
  if (n.ActiveCount() != 0 || n.HistoryCount() != 2) {
    return false;
  }
  for (int i = 0; i < 25; ++i) {
    n.Notify(Kizuri::LogLevel::Warning, "spam");
  }
  if (n.HistoryCount() != 20 || n.ActiveCount() != 25) {
    return false;
  }
  n.SetLifetime(1.0);
  n.Update(1.5);
  if (n.ActiveCount() != 0 || n.HistoryCount() != 20) {
    return false;
  }
  n.Clear();
  return n.ActiveCount() == 0 && n.HistoryCount() == 0;
}
bool TestGuid() {
  std::string a = Kizuri::GenerateGuidString();
  std::string b = Kizuri::GenerateGuidString();
  if (a.size() != 36 || b.size() != 36 || a == b) {
    return false;
  }
  if (a[8] != '-' || a[13] != '-' || a[18] != '-' || a[23] != '-') {
    return false;
  }
  if (Kizuri::Fnv1a64(nullptr, 0) != 14695981039346656037ULL) {
    return false;
  }
  if (Kizuri::Fnv1a64("", 0) != 14695981039346656037ULL) {
    return false;
  }
  const char* hello = "hello";
  if (Kizuri::Fnv1a64(hello, 5) == Kizuri::Fnv1a64(hello, 4)) {
    return false;
  }
  uint64_t h1 = 0;
  uint64_t h2 = 0;
  if (!Kizuri::Fnv1a64File("Samples/Assets/cube.gltf", h1)) {
    return false;
  }
  if (!Kizuri::Fnv1a64File("Samples/Assets/cube.gltf", h2) || h1 != h2 || h1 == 0) {
    return false;
  }
  if (Kizuri::Fnv1a64File("no_such_file_xyz.bin", h2)) {
    return false;
  }
  return true;
}
bool TestTangentGen() {
  std::vector<float> pos = { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 };
  std::vector<float> nrm = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
  std::vector<float> uv = { 0, 0, 1, 0, 1, 1, 0, 1 };
  std::vector<uint32_t> idx = { 0, 1, 2, 0, 2, 3 };
  std::vector<float> tan;
  if (!Kizuri::GenerateTangents(pos, nrm, uv, idx, tan) || tan.size() != 16) {
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (std::fabs(tan[i * 4 + 0] - 1.0f) > 1e-4f || std::fabs(tan[i * 4 + 1]) > 1e-4f || std::fabs(tan[i * 4 + 2]) > 1e-4f || tan[i * 4 + 3] != 1.0f) {
      return false;
    }
  }
  std::vector<float> uvMirror = { 0, 0, 1, 0, 1, 1, 0, 1 };
  for (size_t i = 0; i < 4; ++i) {
    uvMirror[i * 2 + 0] = 1.0f - uvMirror[i * 2 + 0];
  }
  std::vector<float> tanM;
  if (!Kizuri::GenerateTangents(pos, nrm, uvMirror, idx, tanM) || tanM.size() != 16) {
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (std::fabs(tanM[i * 4 + 0] + 1.0f) > 1e-4f || tanM[i * 4 + 3] != -1.0f) {
      return false;
    }
  }
  std::vector<float> bad;
  if (Kizuri::GenerateTangents(pos, nrm, uv, std::vector<uint32_t>{ 0, 1 }, bad)) {
    return false;
  }
  if (Kizuri::GenerateTangents(pos, nrm, std::vector<float>{ 0, 0 }, idx, bad)) {
    return false;
  }
  if (Kizuri::GenerateTangents(pos, nrm, uv, std::vector<uint32_t>{ 0, 1, 99 }, bad)) {
    return false;
  }
  Kizuri::MeshAssetData data;
  data.guid = "01234567-89ab-cdef-0123-456789abcd";
  data.positions = pos;
  data.normals = nrm;
  data.uvs = uv;
  data.indices = idx;
  std::vector<unsigned char> bytes;
  if (Kizuri::EncodeMeshMemory(data, bytes)) {
    return false;
  }
  return true;
}
bool TestMeshCodec() {
  Kizuri::MeshAssetData data;
  data.guid = "01234567-89ab-cdef-0123-456789abcdef";
  data.positions = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f };
  data.normals = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
  data.uvs = { 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f };
  data.indices = { 0, 1, 2 };
  if (!Kizuri::GenerateTangents(data.positions, data.normals, data.uvs, data.indices, data.tangents)) {
    return false;
  }
  if (data.tangents.size() != 12) {
    return false;
  }
  if (std::fabs(data.tangents[0] - 1.0f) > 1e-4f || std::fabs(data.tangents[1]) > 1e-4f || std::fabs(data.tangents[2]) > 1e-4f || data.tangents[3] != 1.0f) {
    return false;
  }
  Kizuri::MeshMaterialData mat;
  mat.name = "Mat1";
  mat.albedo[0] = 0.2f;
  mat.albedo[1] = 0.4f;
  mat.albedo[2] = 0.6f;
  mat.metallic = 0.1f;
  mat.roughness = 0.9f;
  mat.normalTexGuid = "normal-guid-1";
  mat.ormTexGuid = "orm-guid-1";
  data.materials.push_back(mat);
  Kizuri::MeshPartData part;
  part.indexOffset = 0;
  part.indexCount = 3;
  part.material = 0;
  data.parts.push_back(part);
  data.aabbMin[0] = -0.5f;
  data.aabbMin[1] = -0.5f;
  data.aabbMin[2] = 0.0f;
  data.aabbMax[0] = 0.5f;
  data.aabbMax[1] = 0.5f;
  data.aabbMax[2] = 0.0f;
  data.hasSource = true;
  data.sourcePath = "C:/proj/tri.glb";
  data.sourceHash = 123456789ULL;
  data.sourceTimestamp = 987654321LL;
  std::vector<unsigned char> bytes;
  if (!Kizuri::EncodeMeshMemory(data, bytes) || bytes.size() < 16) {
    return false;
  }
  Kizuri::MeshAssetData back;
  if (!Kizuri::DecodeMeshMemory(bytes.data(), bytes.size(), back)) {
    return false;
  }
  if (back.guid != data.guid || back.positions != data.positions || back.normals != data.normals || back.uvs != data.uvs || back.indices != data.indices) {
    return false;
  }
  if (back.tangents != data.tangents) {
    return false;
  }
  if (back.materials.size() != 1 || back.materials[0].name != "Mat1" || back.materials[0].albedo[2] != 0.6f) {
    return false;
  }
  if (back.materials[0].normalTexGuid != "normal-guid-1" || back.materials[0].ormTexGuid != "orm-guid-1") {
    return false;
  }
  if (back.parts.size() != 1 || back.parts[0].indexCount != 3) {
    return false;
  }
  if (back.aabbMin[0] != -0.5f || back.aabbMax[1] != 0.5f) {
    return false;
  }
  if (!back.hasSource || back.sourcePath != data.sourcePath || back.sourceHash != data.sourceHash || back.sourceTimestamp != data.sourceTimestamp) {
    return false;
  }
  std::vector<unsigned char> bad = bytes;
  bad[0] = 'X';
  Kizuri::MeshAssetData tmp;
  if (Kizuri::DecodeMeshMemory(bad.data(), bad.size(), tmp)) {
    return false;
  }
  bad = bytes;
  bad[4] = 0xFF;
  if (Kizuri::DecodeMeshMemory(bad.data(), bad.size(), tmp)) {
    return false;
  }
  if (Kizuri::DecodeMeshMemory(bytes.data(), 10, tmp)) {
    return false;
  }
  const char* path = "test_mesh_tmp.kzmesh";
  std::remove(path);
  if (!Kizuri::EncodeMeshFile(data, path)) {
    return false;
  }
  Kizuri::MeshAssetData fromFile;
  bool ok = Kizuri::DecodeMeshFile(path, fromFile) && fromFile.guid == data.guid && fromFile.indices == data.indices;
  std::remove(path);
  if (!ok) {
    return false;
  }
  if (Kizuri::DecodeMeshFile("no_such_file_xyz.kzmesh", tmp)) {
    return false;
  }
  return true;
}
bool TestMeshImport() {
  Kizuri::MeshAssetData data;
  if (!Kizuri::ImportGltfMesh("Samples/Assets/cube.gltf", "", data, nullptr)) {
    return false;
  }
  if (data.positions.size() / 3 != 24 || data.indices.size() != 36 || data.parts.empty()) {
    return false;
  }
  if (!data.hasSource || data.sourcePath != "Samples/Assets/cube.gltf" || data.sourceHash == 0) {
    return false;
  }
  if (data.aabbMin[0] != -0.5f || data.aabbMax[0] != 0.5f) {
    return false;
  }
  if (data.guid.empty()) {
    return false;
  }
  if (data.tangents.size() != data.positions.size() / 3 * 4) {
    return false;
  }
  Kizuri::MeshAssetData keep;
  if (!Kizuri::ImportGltfMesh("Samples/Assets/cube.gltf", "fixed-guid-1234", keep, nullptr) || keep.guid != "fixed-guid-1234") {
    return false;
  }
  if (Kizuri::ImportGltfMesh("no_such_file_xyz.glb", "", data, nullptr)) {
    return false;
  }
  return true;
}
bool TestAssetDatabase() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzdb_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  fs::copy_file("Samples/Assets/cube.gltf", dir / "cube.gltf", ec);
  if (ec) {
    return false;
  }
  Kizuri::AssetDatabase db;
  db.SetAssetsDir(dir.string());
  db.Scan();
  db.DrainBlocking();
  if (db.PendingImports() != 0) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<std::string> guids = db.AllGuids();
  if (guids.size() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::string guid = guids[0];
  const Kizuri::MeshRecord* rec = db.GetByGuid(guid);
  if (rec == nullptr || !rec->loaded || !rec->hasSource) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (rec->state != Kizuri::MeshAssetState::Ready || rec->data.positions.size() / 3 != 24) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!fs::exists(rec->meshPath, ec)) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  if (db.PendingImports() != 0 || db.AllGuids().size() != 1 || db.AllGuids()[0] != guid) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.RenameAssetFile(guid, "renamed_cube.kzmesh")) {
    fs::remove_all(dir, ec);
    return false;
  }
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->meshPath.find("renamed_cube.kzmesh") == std::string::npos) {
    fs::remove_all(dir, ec);
    return false;
  }
  {
    FILE* fp = std::fopen((dir / "cube.gltf").string().c_str(), "ab");
    std::fputs(" ", fp);
    std::fclose(fp);
  }
  db.Scan();
  db.DrainBlocking();
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::Outdated) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.Reimport(guid)) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.DrainBlocking();
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::Ready || rec->data.positions.size() / 3 != 24) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.AllGuids()[0] != guid) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::create_directories(dir / "Sub", ec);
  fs::copy_file(dir / "cube.gltf", dir / "Sub" / "moved_cube.glb", ec);
  if (ec) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::copy_file(dir / "cube.gltf", fs::temp_directory_path() / "kzdb_backup.glb", ec);
  if (ec) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(dir / "cube.gltf", ec);
  db.Scan();
  db.DrainBlocking();
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (rec->sourcePath.find("moved_cube.glb") == std::string::npos) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.TakeRelocated().empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(dir / "Sub" / "moved_cube.glb", ec);
  db.Scan();
  db.DrainBlocking();
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::SourceMissing || !rec->loaded) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::copy_file(fs::temp_directory_path() / "kzdb_backup.glb", dir / "Sub" / "moved2.glb", ec);
  if (ec) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(fs::temp_directory_path() / "kzdb_backup.glb", ec);
  if (db.RelocateMissing() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.DrainBlocking();
  rec = db.GetByGuid(guid);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (rec->sourcePath.find("moved2.glb") == std::string::npos) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.TakeRelocated().empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(fs::temp_directory_path() / "kzdb_backup.glb", ec);
  if (db.Reimport("no-such-guid")) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<Kizuri::RefUse> refs;
  refs.push_back({ "Hero", guid });
  refs.push_back({ "Sword", guid });
  refs.push_back({ "Tree", "other-guid" });
  std::map<std::string, size_t> counts = db.ComputeRefCounts(refs);
  if (counts[guid] != 2) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<std::string> blocked;
  if (db.DeleteAssetFile(guid, false, refs, blocked)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (blocked.size() != 2 || blocked[0] != "Hero" || blocked[1] != "Sword") {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.GetByGuid(guid) == nullptr) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<std::string> blocked2;
  if (!db.DeleteAssetFile(guid, true, refs, blocked2) || db.GetByGuid(guid) != nullptr) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshAssetData orphan;
  orphan.guid = Kizuri::GenerateGuidString();
  orphan.positions = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
  orphan.normals = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
  orphan.uvs = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
  orphan.indices = { 0, 1, 2 };
  if (!Kizuri::GenerateTangents(orphan.positions, orphan.normals, orphan.uvs, orphan.indices, orphan.tangents)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshPartData opart;
  opart.indexOffset = 0;
  opart.indexCount = 3;
  opart.material = 0;
  orphan.parts.push_back(opart);
  orphan.aabbMin[0] = 0.0f;
  orphan.aabbMin[1] = 0.0f;
  orphan.aabbMin[2] = 0.0f;
  orphan.aabbMax[0] = 1.0f;
  orphan.aabbMax[1] = 1.0f;
  orphan.aabbMax[2] = 0.0f;
  orphan.hasSource = false;
  if (!Kizuri::EncodeMeshFile(orphan, (dir / "orphan.kzmesh").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  const Kizuri::MeshRecord* orec = db.GetByGuid(orphan.guid);
  if (orec == nullptr || orec->state != Kizuri::MeshAssetState::NoSource || !orec->loaded) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (orec->data.positions.size() != 9) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.Reimport(orphan.guid)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.RenameAssetFile(orphan.guid, "renamed_orphan.kzmesh")) {
    fs::remove_all(dir, ec);
    return false;
  }
  orec = db.GetByGuid(orphan.guid);
  if (orec == nullptr || orec->meshPath.find("renamed_orphan.kzmesh") == std::string::npos) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestImportNoRetry() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kznoretry_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  {
    FILE* fp = std::fopen((dir / "garbage.glb").string().c_str(), "wb");
    std::fputs("NOT GLTF {{{{", fp);
    std::fclose(fp);
  }
  Kizuri::AssetDatabase db;
  db.SetAssetsDir(dir.string());
  db.Scan();
  db.DrainBlocking();
  if (!db.AllGuids().empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  if (db.PendingImports() != 0 || !db.AllGuids().empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::copy_file("Samples/Assets/cube.gltf", dir / "garbage.glb", fs::copy_options::overwrite_existing, ec);
  if (ec) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  db.Scan();
  db.DrainBlocking();
  if (db.AllGuids().size() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  const Kizuri::MeshRecord* rec = db.GetByGuid(db.AllGuids()[0]);
  if (rec == nullptr || rec->state != Kizuri::MeshAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestSceneMeshGuid() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("WithMesh");
  Kizuri::Entity* e = scene.Get(a);
  e->meshGuid = "guid-abc-123";
  e->hasMesh = true;
  scene.CreateEntity("NoMesh");
  const char* path = "test_meshguid_tmp.kzscene";
  std::remove(path);
  if (!Kizuri::SaveSceneToFile(scene, path)) {
    return false;
  }
  Kizuri::Scene back;
  if (!Kizuri::LoadSceneFromFile(back, path, nullptr)) {
    std::remove(path);
    return false;
  }
  std::remove(path);
  const Kizuri::Entity* ra = nullptr;
  const Kizuri::Entity* rn = nullptr;
  std::vector<Kizuri::EntityId> all = back.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* ce = back.Get(all[i]);
    if (ce->name == "WithMesh") {
      ra = ce;
    } else {
      rn = ce;
    }
  }
  if (ra == nullptr || rn == nullptr) {
    return false;
  }
  return ra->meshGuid == "guid-abc-123" && ra->hasMesh && rn->meshGuid.empty() && !rn->hasMesh;
}
void WriteTestBMP(const std::string& path, int w, int h, bool withAlpha) {
  int rowBytes = w * 4;
  int imgBytes = rowBytes * h;
  std::vector<unsigned char> file(54 + imgBytes, 0);
  file[0] = 'B';
  file[1] = 'M';
  uint32_t fs = static_cast<uint32_t>(file.size());
  file[2] = static_cast<unsigned char>(fs & 0xFF);
  file[3] = static_cast<unsigned char>((fs >> 8) & 0xFF);
  file[4] = static_cast<unsigned char>((fs >> 16) & 0xFF);
  file[5] = static_cast<unsigned char>((fs >> 24) & 0xFF);
  file[10] = 54;
  file[14] = 40;
  file[18] = static_cast<unsigned char>(w & 0xFF);
  file[19] = static_cast<unsigned char>((w >> 8) & 0xFF);
  file[22] = static_cast<unsigned char>(h & 0xFF);
  file[23] = static_cast<unsigned char>((h >> 8) & 0xFF);
  file[26] = 1;
  file[28] = 32;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      size_t o = 54 + static_cast<size_t>(h - 1 - y) * rowBytes + x * 4;
      file[o + 0] = static_cast<unsigned char>((x * 64) & 0xFF);
      file[o + 1] = static_cast<unsigned char>((y * 64) & 0xFF);
      file[o + 2] = 128;
      file[o + 3] = (withAlpha && x == 0 && y == 0) ? 0 : 255;
    }
  }
  FILE* fp = std::fopen(path.c_str(), "wb");
  std::fwrite(file.data(), 1, file.size(), fp);
  std::fclose(fp);
}
bool TestTexCodec() {
  Kizuri::TextureAssetData data;
  data.guid = "tex-guid-0001";
  data.width = 4;
  data.height = 4;
  data.format = Kizuri::TexFormat::Rgba8;
  data.srgb = true;
  for (int m = 0; m < 3; ++m) {
    int mw = 4 >> m;
    int mh = 4 >> m;
    Kizuri::TextureMipData mip;
    mip.width = static_cast<uint32_t>(mw);
    mip.height = static_cast<uint32_t>(mh);
    mip.rowPitch = static_cast<uint32_t>(mw * 4);
    mip.data.resize(mip.rowPitch * mh, static_cast<unsigned char>(m + 1));
    data.mips.push_back(mip);
  }
  data.hasSource = true;
  data.sourcePath = "C:/proj/a.png";
  data.sourceHash = 777ULL;
  data.sourceTimestamp = 888LL;
  std::vector<unsigned char> bytes;
  if (!Kizuri::EncodeTextureMemory(data, bytes)) {
    return false;
  }
  Kizuri::TextureAssetData back;
  if (!Kizuri::DecodeTextureMemory(bytes.data(), bytes.size(), back)) {
    return false;
  }
  if (back.guid != data.guid || back.width != 4 || back.mips.size() != 3 || back.format != Kizuri::TexFormat::Rgba8 || !back.srgb) {
    return false;
  }
  if (back.mips[2].data.size() != 4 || back.mips[2].data[0] != 3) {
    return false;
  }
  if (back.sourceHash != 777ULL || back.sourceTimestamp != 888LL) {
    return false;
  }
  std::vector<unsigned char> bad = bytes;
  bad[0] = 'X';
  Kizuri::TextureAssetData tmp;
  if (Kizuri::DecodeTextureMemory(bad.data(), bad.size(), tmp)) {
    return false;
  }
  const char* path = "test_tex_tmp.kztex";
  std::remove(path);
  if (!Kizuri::EncodeTextureFile(data, path)) {
    return false;
  }
  Kizuri::TextureAssetData fromFile;
  bool ok = Kizuri::DecodeTextureFile(path, fromFile) && fromFile.guid == data.guid;
  std::remove(path);
  return ok;
}
bool TestTexImportOpaque() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kztexopaque_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  std::string opaque = (dir / "brick.bmp").string();
  WriteTestBMP(opaque, 8, 8, false);
  Kizuri::TextureAssetData to;
  if (!Kizuri::ImportTextureFile(opaque, "", to)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (to.format != Kizuri::TexFormat::Bc1 || !to.srgb || to.mips.size() < 3) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (to.width != 8 || to.mips[0].width != 8 || to.mips.back().width != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestTexImportAlpha() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kztexalpha_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  std::string alpha = (dir / "glass.bmp").string();
  WriteTestBMP(alpha, 8, 8, true);
  Kizuri::TextureAssetData ta;
  if (!Kizuri::ImportTextureFile(alpha, "", ta)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (ta.format != Kizuri::TexFormat::Bc3) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestTexImportNormal() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kztexnormal_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  std::string normal = (dir / "wall_normal.bmp").string();
  WriteTestBMP(normal, 8, 8, false);
  Kizuri::TextureAssetData tn;
  if (!Kizuri::ImportTextureFile(normal, "", tn)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (tn.format != Kizuri::TexFormat::Bc5 || tn.srgb) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (Kizuri::ImportTextureFile("no_such_file_xyz.bmp", "", tn)) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestTexImport() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzteximport_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  std::string opaque = (dir / "brick.bmp").string();
  std::string alpha = (dir / "glass.bmp").string();
  std::string normal = (dir / "wall_normal.bmp").string();
  WriteTestBMP(opaque, 8, 8, false);
  WriteTestBMP(alpha, 8, 8, true);
  WriteTestBMP(normal, 8, 8, false);
  Kizuri::TextureAssetData to;
  Kizuri::TextureAssetData ta;
  Kizuri::TextureAssetData tn;
  if (!Kizuri::ImportTextureFile(opaque, "", to)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!Kizuri::ImportTextureFile(alpha, "", ta)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!Kizuri::ImportTextureFile(normal, "", tn)) {
    fs::remove_all(dir, ec);
    return false;
  }
  bool ok = true;
  if (to.format != Kizuri::TexFormat::Bc1 || !to.srgb || to.mips.size() < 3) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (ta.format != Kizuri::TexFormat::Bc3) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (tn.format != Kizuri::TexFormat::Bc5 || tn.srgb) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (to.width != 8 || to.mips[0].width != 8 || to.mips.back().width != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (Kizuri::ImportTextureFile("no_such_file_xyz.bmp", "", tn)) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestRHITextures() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 64;
  desc.height = 64;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::RHITexture tex = rhi->CreateTexture2D(64, 64, 7, Kizuri::RHITextureFormat::RGBA8_UNORM);
  if (tex == 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->CreateTexture2D(0, 64, 1, Kizuri::RHITextureFormat::RGBA8_UNORM) != 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  std::vector<unsigned char> px(64 * 64 * 4, 127);
  if (!rhi->UpdateTextureMip(tex, 0, 64, 64, 256, px.data(), px.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  std::vector<unsigned char> bad(10, 0);
  if (rhi->UpdateTextureMip(tex, 0, 64, 64, 256, bad.data(), bad.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (!rhi->UpdateTextureMip(tex, 0, 64, 64, 256, px.data(), px.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->TextureResidentMips(tex) != 1) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->UpdateTextureMip(tex, 7, 1, 1, 4, px.data(), 4)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::RHITexture bc = rhi->CreateTexture2D(8, 8, 4, Kizuri::RHITextureFormat::BC1_UNORM);
  std::vector<unsigned char> blk(32, 0);
  if (!rhi->UpdateTextureMip(bc, 0, 8, 8, 16, blk.data(), blk.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->UpdateTextureMip(bc, 0, 8, 8, 8, blk.data(), 16)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->DestroyTexture(tex);
  rhi->DestroyTexture(bc);
  if (rhi->TextureResidentMips(tex) != 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
}
bool TestTexStreaming() {
  if (Kizuri::AssetDatabase::SelectMipLevel(5.0f, 7) != 7) {
    return false;
  }
  if (Kizuri::AssetDatabase::SelectMipLevel(20.0f, 7) != 6) {
    return false;
  }
  if (Kizuri::AssetDatabase::SelectMipLevel(50.0f, 7) != 2) {
    return false;
  }
  if (Kizuri::AssetDatabase::SelectMipLevel(200.0f, 7) != 1) {
    return false;
  }
  if (Kizuri::AssetDatabase::SelectMipLevel(200.0f, 1) != 1) {
    return false;
  }
  if (Kizuri::AssetDatabase::SelectMipLevel(5.0f, 0) != 0) {
    return false;
  }
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzstream_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  WriteTestBMP((dir / "wall.bmp").string(), 32, 32, false);
  Kizuri::AssetDatabase db;
  db.SetAssetsDir(dir.string());
  db.Scan();
  db.DrainBlocking();
  if (db.AllTexGuids().size() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::string tguid = db.AllTexGuids()[0];
  Kizuri::MeshAssetData mesh;
  mesh.guid = "stream-mesh-1";
  mesh.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
  mesh.normals = { 0, 0, 1, 0, 0, 1, 0, 0, 1 };
  mesh.uvs = { 0, 0, 1, 0, 0, 1 };
  mesh.indices = { 0, 1, 2 };
  if (!Kizuri::GenerateTangents(mesh.positions, mesh.normals, mesh.uvs, mesh.indices, mesh.tangents)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshMaterialData mat;
  mat.name = "M";
  mat.albedo[0] = 1.0f;
  mat.albedo[1] = 1.0f;
  mat.albedo[2] = 1.0f;
  mat.metallic = 0.0f;
  mat.roughness = 0.5f;
  mat.albedoTexGuid = tguid;
  mesh.materials.push_back(mat);
  Kizuri::MeshPartData part;
  part.indexOffset = 0;
  part.indexCount = 3;
  part.material = 0;
  mesh.parts.push_back(part);
  mesh.aabbMin[0] = 0.0f;
  mesh.aabbMin[1] = 0.0f;
  mesh.aabbMin[2] = 0.0f;
  mesh.aabbMax[0] = 1.0f;
  mesh.aabbMax[1] = 1.0f;
  mesh.aabbMax[2] = 0.0f;
  mesh.hasSource = false;
  if (!Kizuri::EncodeMeshFile(mesh, (dir / "stream.kzmesh").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  const Kizuri::MeshRecord* mr = db.GetByGuid("stream-mesh-1");
  if (mr == nullptr || !mr->loaded) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 64;
  desc.height = 64;
  desc.vsync = false;
  rhi->Initialize(desc);
  float camFar[3] = { 500.0f, 0.0f, 0.0f };
  float obj[3] = { 0.0f, 0.0f, 0.0f };
  std::vector<Kizuri::MeshUse> uses;
  Kizuri::MeshUse use;
  use.meshGuid = "stream-mesh-1";
  use.pos[0] = 0.0f;
  use.pos[1] = 0.0f;
  use.pos[2] = 0.0f;
  uses.push_back(use);
  (void)obj;
  db.UpdateStreaming(camFar, uses);
  const Kizuri::TextureRecord* tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->selectedLevels != 1) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  size_t d1 = db.DrainUploads(rhi, 1000000);
  tr = db.GetTexByGuid(tguid);
  if (d1 != 1 || tr->residentLevels != 1) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  float camNear[3] = { 2.0f, 0.0f, 0.0f };
  db.UpdateStreaming(camNear, uses);
  tr = db.GetTexByGuid(tguid);
  int full = static_cast<int>(tr->data.mips.size());
  if (tr->selectedLevels != full) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  size_t d2 = db.DrainUploads(rhi, 1000000);
  tr = db.GetTexByGuid(tguid);
  if (tr->residentLevels != full || d2 != static_cast<size_t>(full - 1)) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  size_t d3 = db.DrainUploads(rhi, 1);
  if (d3 != 0) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::DestroyRHI(rhi);
  fs::remove_all(dir, ec);
  return true;
}
bool TestGltfTextured() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzgltftex_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  WriteTestBMP((dir / "t.bmp").string(), 4, 4, false);
  WriteTestBMP((dir / "t_nrm.bmp").string(), 4, 4, false);
  float tri[9] = { -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
  {
    FILE* fp = std::fopen((dir / "tri.bin").string().c_str(), "wb");
    std::fwrite(tri, 1, sizeof(tri), fp);
    std::fclose(fp);
  }
  std::string gltf = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],\"materials\":[{\"name\":\"M\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}},\"normalTexture\":{\"index\":1}}],\"textures\":[{\"source\":0},{\"source\":1}],\"images\":[{\"uri\":\"t.bmp\"},{\"uri\":\"t_nrm.bmp\"}],\"buffers\":[{\"uri\":\"tri.bin\",\"byteLength\":36}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}]}";
  {
    FILE* fp = std::fopen((dir / "tri.gltf").string().c_str(), "wb");
    std::fwrite(gltf.c_str(), 1, gltf.size(), fp);
    std::fclose(fp);
  }
  Kizuri::MeshAssetData data;
  if (!Kizuri::ImportGltfMesh((dir / "tri.gltf").string(), "", data, nullptr)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (data.positions.size() != 9 || data.indices.size() != 3) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (data.materials.empty() || data.materials[0].albedoTexGuid.empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::string texFile = (dir / "tri_tex0.kztex").string();
  if (!fs::exists(texFile, ec)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::TextureAssetData tex;
  if (!Kizuri::DecodeTextureFile(texFile, tex) || tex.guid != data.materials[0].albedoTexGuid) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (data.materials[0].normalTexGuid.empty()) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::string nrmFile = (dir / "tri_tex1.kztex").string();
  if (!fs::exists(nrmFile, ec)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::TextureAssetData nrmTex;
  if (!Kizuri::DecodeTextureFile(nrmFile, nrmTex) || nrmTex.guid != data.materials[0].normalTexGuid) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (nrmTex.format != Kizuri::TexFormat::Bc5 || nrmTex.srgb) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  return true;
}
bool TestLightDefaults() {
  Kizuri::LightData l;
  Kizuri::MakeDefaultLight(l);
  if (l.type != static_cast<int>(Kizuri::LightType::Point)) {
    return false;
  }
  if (l.intensity != 3.0f || l.radius != 10.0f || l.spotAngle != 45.0f || l.falloff != 0.1f) {
    return false;
  }
  if (l.castShadow || l.lightSize != 0.3f || l.shadowSize != 1024 || l.softness != 0.3f) {
    return false;
  }
  if (l.cascades != 3 || l.lambda != 0.5f) {
    return false;
  }
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  if (scene.Get(a)->hasLight) {
    return false;
  }
  return true;
}
bool TestLightTypeRetention() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  std::unique_ptr<Kizuri::Command> add(new Kizuri::AddLightCmd(a));
  if (!undo.Execute(std::move(add), scene)) {
    return false;
  }
  Kizuri::LightData spot = scene.Get(a)->light;
  spot.type = static_cast<int>(Kizuri::LightType::Spot);
  spot.spotAngle = 60.0f;
  spot.radius = 30.0f;
  std::unique_ptr<Kizuri::Command> setSpot(new Kizuri::SetLightCmd(a, scene.Get(a)->light, spot));
  if (!undo.Execute(std::move(setSpot), scene)) {
    return false;
  }
  Kizuri::LightData point = scene.Get(a)->light;
  point.type = static_cast<int>(Kizuri::LightType::Point);
  std::unique_ptr<Kizuri::Command> setPoint(new Kizuri::SetLightCmd(a, scene.Get(a)->light, point));
  if (!undo.Execute(std::move(setPoint), scene)) {
    return false;
  }
  if (scene.Get(a)->light.spotAngle != 60.0f || scene.Get(a)->light.radius != 30.0f) {
    return false;
  }
  return true;
}
bool TestMaterialDefaults() {
  Kizuri::MaterialData m;
  Kizuri::MakeDefaultMaterial(m);
  if (m.albedo[0] != 1.0f || m.albedo[1] != 1.0f || m.albedo[2] != 1.0f) {
    return false;
  }
  if (m.metallic != 0.0f || m.roughness != 0.5f) {
    return false;
  }
  if (!m.albedoTex.empty() || !m.normalTex.empty() || !m.ormTex.empty()) {
    return false;
  }
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  if (scene.Get(a)->hasMaterial) {
    return false;
  }
  return true;
}
bool TestMaterialRoundTrip() {
  Kizuri::Scene scene;
  Kizuri::EntityId m = scene.CreateEntity("Mat");
  scene.Get(m)->hasMaterial = true;
  Kizuri::MakeDefaultMaterial(scene.Get(m)->material);
  scene.Get(m)->material.albedo[0] = 0.8f;
  scene.Get(m)->material.albedo[1] = 0.2f;
  scene.Get(m)->material.albedo[2] = 0.1f;
  scene.Get(m)->material.metallic = 0.9f;
  scene.Get(m)->material.roughness = 0.25f;
  scene.Get(m)->material.albedoTex = "albedo-guid-1";
  scene.Get(m)->material.normalTex = "normal-guid-2";
  scene.Get(m)->material.ormTex = "orm-guid-3";
  Kizuri::EntityId p = scene.CreateEntity("Plain");
  (void)p;
  const char* path = "test_material_tmp.kzscene";
  std::remove(path);
  if (!Kizuri::SaveSceneToFile(scene, path)) {
    return false;
  }
  Kizuri::Scene back;
  if (!Kizuri::LoadSceneFromFile(back, path, nullptr)) {
    std::remove(path);
    return false;
  }
  std::remove(path);
  const Kizuri::Entity* rm = nullptr;
  const Kizuri::Entity* rp = nullptr;
  std::vector<Kizuri::EntityId> all = back.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* ce = back.Get(all[i]);
    if (ce->name == "Mat") {
      rm = ce;
    } else if (ce->name == "Plain") {
      rp = ce;
    }
  }
  if (rm == nullptr || rp == nullptr) {
    return false;
  }
  if (!rm->hasMaterial) {
    return false;
  }
  if (std::fabs(rm->material.albedo[0] - 0.8f) > 1e-6f || std::fabs(rm->material.metallic - 0.9f) > 1e-6f || std::fabs(rm->material.roughness - 0.25f) > 1e-6f) {
    return false;
  }
  if (rm->material.albedoTex != "albedo-guid-1" || rm->material.normalTex != "normal-guid-2" || rm->material.ormTex != "orm-guid-3") {
    return false;
  }
  if (rp->hasMaterial) {
    return false;
  }
  return true;
}
bool TestMaterialUndo() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EditQueue edits;
  Kizuri::EntityId a = scene.CreateEntity("A");
  std::unique_ptr<Kizuri::Command> add(new Kizuri::AddMaterialCmd(a));
  if (!undo.Execute(std::move(add), scene) || !scene.Get(a)->hasMaterial) {
    return false;
  }
  if (scene.Get(a)->material.roughness != 0.5f) {
    return false;
  }
  Kizuri::MaterialData v = scene.Get(a)->material;
  v.roughness = 0.9f;
  v.albedoTex = "tex-a";
  edits.PushMaterial(a, v);
  Kizuri::MaterialData v2 = v;
  v2.roughness = 0.1f;
  v2.normalTex = "tex-n";
  edits.PushMaterial(a, v2);
  if (edits.Pending() != 2) {
    return false;
  }
  if (edits.ApplyAll(scene, undo) != 1) {
    return false;
  }
  if (scene.Get(a)->material.roughness != 0.1f || scene.Get(a)->material.normalTex != "tex-n") {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->material.roughness != 0.5f || !scene.Get(a)->material.albedoTex.empty()) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->material.roughness != 0.1f) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> rem(new Kizuri::RemoveMaterialCmd(a));
  if (!undo.Execute(std::move(rem), scene) || scene.Get(a)->hasMaterial) {
    return false;
  }
  if (!undo.Undo(scene) || !scene.Get(a)->hasMaterial || scene.Get(a)->material.roughness != 0.1f) {
    return false;
  }
  return true;
}
bool TestLightRoundTrip() {
  Kizuri::Scene scene;
  Kizuri::EntityId p = scene.CreateEntity("Point");
  scene.Get(p)->hasLight = true;
  Kizuri::MakeDefaultLight(scene.Get(p)->light);
  Kizuri::EntityId s = scene.CreateEntity("Spot");
  scene.Get(s)->hasLight = true;
  Kizuri::MakeDefaultLight(scene.Get(s)->light);
  scene.Get(s)->light.type = static_cast<int>(Kizuri::LightType::Spot);
  scene.Get(s)->light.spotAngle = 60.0f;
  scene.Get(s)->light.falloff = 0.2f;
  scene.Get(s)->light.castShadow = true;
  scene.Get(s)->light.lightSize = 0.4f;
  scene.Get(s)->light.shadowSize = 512;
  scene.Get(s)->light.softness = 0.7f;
  Kizuri::EntityId d = scene.CreateEntity("Sun");
  scene.Get(d)->hasLight = true;
  Kizuri::MakeDefaultLight(scene.Get(d)->light);
  scene.Get(d)->light.type = static_cast<int>(Kizuri::LightType::Directional);
  scene.Get(d)->light.intensity = 2.5f;
  scene.Get(d)->light.castShadow = true;
  scene.Get(d)->light.cascades = 2;
  scene.Get(d)->light.lambda = 0.7f;
  scene.Get(d)->light.softness = 0.5f;
  scene.Get(d)->light.lightSize = 0.5f;
  const char* path = "test_light_tmp.kzscene";
  std::remove(path);
  if (!Kizuri::SaveSceneToFile(scene, path)) {
    return false;
  }
  Kizuri::Scene back;
  if (!Kizuri::LoadSceneFromFile(back, path, nullptr)) {
    std::remove(path);
    return false;
  }
  std::remove(path);
  const Kizuri::Entity* rp = nullptr;
  const Kizuri::Entity* rs = nullptr;
  const Kizuri::Entity* rd = nullptr;
  std::vector<Kizuri::EntityId> all = back.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* ce = back.Get(all[i]);
    if (ce->name == "Point") {
      rp = ce;
    } else if (ce->name == "Spot") {
      rs = ce;
    } else if (ce->name == "Sun") {
      rd = ce;
    }
  }
  if (rp == nullptr || rs == nullptr || rd == nullptr) {
    return false;
  }
  if (!rp->hasLight || rp->light.type != static_cast<int>(Kizuri::LightType::Point) || rp->light.castShadow) {
    return false;
  }
  if (!rs->hasLight || rs->light.spotAngle != 60.0f || rs->light.falloff != 0.2f) {
    return false;
  }
  if (!rs->light.castShadow || rs->light.shadowSize != 512 || std::fabs(rs->light.softness - 0.7f) > 1e-6f) {
    return false;
  }
  if (!rd->hasLight || rd->light.type != static_cast<int>(Kizuri::LightType::Directional)) {
    return false;
  }
  if (!rd->light.castShadow || rd->light.cascades != 2 || std::fabs(rd->light.lambda - 0.7f) > 1e-6f) {
    return false;
  }
  return true;
}
bool TestLightUndo() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EditQueue edits;
  Kizuri::EntityId a = scene.CreateEntity("A");
  std::unique_ptr<Kizuri::Command> add(new Kizuri::AddLightCmd(a));
  if (!undo.Execute(std::move(add), scene) || !scene.Get(a)->hasLight) {
    return false;
  }
  if (scene.Get(a)->light.type != static_cast<int>(Kizuri::LightType::Point)) {
    return false;
  }
  Kizuri::LightData v = scene.Get(a)->light;
  v.intensity = 9.0f;
  edits.PushLight(a, v);
  Kizuri::LightData v2 = v;
  v2.intensity = 10.0f;
  edits.PushLight(a, v2);
  if (edits.Pending() != 2) {
    return false;
  }
  if (edits.ApplyAll(scene, undo) != 1) {
    return false;
  }
  if (scene.Get(a)->light.intensity != 10.0f) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->light.intensity != 3.0f) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->light.intensity != 10.0f) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> rem(new Kizuri::RemoveLightCmd(a));
  if (!undo.Execute(std::move(rem), scene) || scene.Get(a)->hasLight) {
    return false;
  }
  if (!undo.Undo(scene) || !scene.Get(a)->hasLight || scene.Get(a)->light.intensity != 10.0f) {
    return false;
  }
  return true;
}
bool TestEntityForward() {
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  float dir[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::EntityForward(t, dir);
  if (std::fabs(dir[0]) > 0.01f || std::fabs(dir[1]) > 0.01f || std::fabs(dir[2] - 1.0f) > 0.01f) {
    return false;
  }
  t.rotation[1] = 90.0f;
  Kizuri::EntityForward(t, dir);
  if (std::fabs(dir[0] - 1.0f) > 0.01f || std::fabs(dir[1]) > 0.01f || std::fabs(dir[2]) > 0.01f) {
    return false;
  }
  t.rotation[1] = 0.0f;
  t.rotation[0] = 90.0f;
  Kizuri::EntityForward(t, dir);
  if (std::fabs(dir[0]) > 0.01f || std::fabs(dir[1] + 1.0f) > 0.01f || std::fabs(dir[2]) > 0.01f) {
    return false;
  }
  return true;
}
bool TestPBRPointSpot() {
  float albedo[3] = { 0.8f, 0.2f, 0.15f };
  float N[3] = { 0.0f, 1.0f, 0.0f };
  float V[3] = { 0.3f, 0.9f, 0.2f };
  float L[3] = { 0.4f, 0.9f, 0.1f };
  float light[3] = { 3.0f, 3.0f, 3.0f };
  float out[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::PBR_Point(albedo, 0.5f, 0.0f, N, V, L, 5.0f, 10.0f, light, out);
  if (!(out[0] > 0.0f && out[1] > 0.0f && out[2] > 0.0f)) {
    return false;
  }
  float far[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::PBR_Point(albedo, 0.5f, 0.0f, N, V, L, 50.0f, 10.0f, light, far);
  if (!(far[0] == 0.0f && far[1] == 0.0f && far[2] == 0.0f)) {
    return false;
  }
  float N2[3] = { 0.0f, 1.0f, 0.0f };
  float V2[3] = { 0.0f, 1.0f, 0.0f };
  float L2[3] = { 0.0f, 1.0f, 0.0f };
  float sd2[3] = { 0.0f, -1.0f, 0.0f };
  float hit[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::PBR_Spot(albedo, 0.5f, 0.0f, N2, V2, L2, 5.0f, 10.0f, sd2, 0.9f, 0.1f, light, hit);
  if (!(hit[0] > 0.0f)) {
    return false;
  }
  float sd3[3] = { 0.0f, 1.0f, 0.0f };
  float miss[3] = { 1.0f, 1.0f, 1.0f };
  Kizuri::PBR_Spot(albedo, 0.5f, 0.0f, N2, V2, L2, 5.0f, 10.0f, sd3, 0.9f, 0.1f, light, miss);
  if (!(miss[0] == 0.0f && miss[1] == 0.0f && miss[2] == 0.0f)) {
    return false;
  }
  return true;
}
bool TestLightGizmo() {
  DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(0.0f, 1.5f, -6.0f, 1.0f), DirectX::XMVectorSet(0.0f, 1.5f, 0.0f, 1.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
  DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(1.04719755f, 16.0f / 9.0f, 0.1f, 500.0f);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  float wpos[3] = { 0.0f, 1.5f, 0.0f };
  float sx = 0.0f;
  float sy = 0.0f;
  if (!Kizuri::ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], wpos, 0.0f, 0.0f, 1280.0f, 720.0f, sx, sy)) {
    return false;
  }
  if (std::fabs(sx - 640.0f) > 1.0f || std::fabs(sy - 360.0f) > 1.0f) {
    return false;
  }
  float behind[3] = { 0.0f, 1.5f, -10.0f };
  if (Kizuri::ProjectWorldToScreen(&vf.m[0][0], &pf.m[0][0], behind, 0.0f, 0.0f, 1280.0f, 720.0f, sx, sy)) {
    return false;
  }
  float c[3] = { 1.0f, 2.0f, 3.0f };
  float sp[144][3];
  Kizuri::LightSpherePoints(c, 2.0f, sp);
  for (int i = 0; i < 144; ++i) {
    float dx = sp[i][0] - c[0];
    float dy = sp[i][1] - c[1];
    float dz = sp[i][2] - c[2];
    float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (std::fabs(d - 2.0f) > 1e-3f) {
      return false;
    }
  }
  float apex[3] = { 0.0f, 5.0f, 0.0f };
  float dir[3] = { 0.0f, -1.0f, 0.0f };
  float cp[40][3];
  Kizuri::LightConePoints(apex, dir, 60.0f, 10.0f, cp);
  float rimR = std::tan(60.0f * 0.5f * 0.01745329252f) * 10.0f;
  float rcx = apex[0] + dir[0] * 10.0f;
  float rcy = apex[1] + dir[1] * 10.0f;
  float rcz = apex[2] + dir[2] * 10.0f;
  for (int i = 0; i < 32; i += 2) {
    float dx = cp[i][0] - rcx;
    float dy = cp[i][1] - rcy;
    float dz = cp[i][2] - rcz;
    if (std::fabs(std::sqrt(dx * dx + dy * dy + dz * dz) - rimR) > 1e-3f) {
      return false;
    }
  }
  for (int g = 32; g < 40; g += 2) {
    if (std::fabs(cp[g][0] - apex[0]) > 1e-4f || std::fabs(cp[g][1] - apex[1]) > 1e-4f || std::fabs(cp[g][2] - apex[2]) > 1e-4f) {
      return false;
    }
  }
  float org[3] = { 0.0f, 0.0f, 0.0f };
  float dd[3] = { 0.0f, -1.0f, 0.0f };
  float ap[6][3];
  Kizuri::LightArrowPoints(org, dd, 3.0f, ap);
  float tipDx = ap[1][0] - org[0];
  float tipDy = ap[1][1] - org[1];
  float tipDz = ap[1][2] - org[2];
  if (std::fabs(std::sqrt(tipDx * tipDx + tipDy * tipDy + tipDz * tipDz) - 3.0f) > 1e-4f) {
    return false;
  }
  if (!(ap[3][1] > ap[1][1])) {
    return false;
  }
  return true;
}
bool TestShaderCompile() {
#ifdef _WIN32
  struct Entry {
    const char* file;
    const char* profile;
  };
  Entry entries[12] = {
    { "GeometryVS.hlsl", "vs_5_0" },
    { "GeometryPS.hlsl", "ps_5_0" },
    { "LightingVS.hlsl", "vs_5_0" },
    { "LightingPS.hlsl", "ps_5_0" },
    { "DepthVS.hlsl", "vs_5_0" },
    { "BloomBrightPS.hlsl", "ps_5_0" },
    { "BloomBlurPS.hlsl", "ps_5_0" },
    { "BloomAddPS.hlsl", "ps_5_0" },
    { "FsrEasuPS.hlsl", "ps_5_0" },
    { "FsrRcasPS.hlsl", "ps_5_0" },
    { "SsaoPS.hlsl", "ps_5_0" },
    { "SsaoBlurPS.hlsl", "ps_5_0" }
  };
  const char* dirs[4] = { "Shaders", "../Shaders", "../../Shaders", "build/bin/Release/Shaders" };
  for (int e = 0; e < 12; ++e) {
    bool found = false;
    for (int d = 0; d < 4; ++d) {
      std::string path = std::string(dirs[d]) + "/" + entries[e].file;
      if (!std::filesystem::exists(path)) {
        continue;
      }
      wchar_t wpath[512];
      size_t conv = 0;
      if (mbstowcs_s(&conv, wpath, 512, path.c_str(), _TRUNCATE) != 0) {
        return false;
      }
      ID3DBlob* code = nullptr;
      ID3DBlob* errs = nullptr;
      HRESULT hr = D3DCompileFromFile(wpath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", entries[e].profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &code, &errs);
      if (errs != nullptr) {
        std::printf("%s: %s\n", path.c_str(), static_cast<const char*>(errs->GetBufferPointer()));
        errs->Release();
      }
      if (code != nullptr) {
        code->Release();
      }
      found = true;
      if (FAILED(hr)) {
        return false;
      }
    }
    if (!found) {
      return false;
    }
  }
#endif
  return true;
}
bool TestShadowSplits() {
  float s[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  Kizuri::ShadowSplitDepths(1.0f, 100.0f, 3, 0.5f, s);
  if (s[0] != 1.0f || s[3] != 100.0f) {
    return false;
  }
  if (!(s[0] < s[1] && s[1] < s[2] && s[2] < s[3])) {
    return false;
  }
  float logMid = 1.0f * std::pow(100.0f, 1.0f / 3.0f);
  float uniMid = 1.0f + 99.0f / 3.0f;
  if (std::fabs(s[1] - (0.5f * logMid + 0.5f * uniMid)) > 0.01f) {
    return false;
  }
  float u[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  Kizuri::ShadowSplitDepths(1.0f, 100.0f, 2, 0.0f, u);
  if (std::fabs(u[1] - 50.5f) > 0.01f) {
    return false;
  }
  return true;
}
bool TestShadowSunMatrix() {
  float sunDir[3] = { 0.4f, -1.0f, 0.3f };
  float camPos[3] = { 0.0f, 5.0f, -10.0f };
  float camFwd[3] = { 0.0f, -0.4f, 0.9f };
  float camRight[3] = { 1.0f, 0.0f, 0.0f };
  float camUp[3] = { 0.0f, 0.9f, 0.4f };
  float vp[16];
  float cn = 0.0f;
  float cf = 0.0f;
  float ce = 0.0f;
  Kizuri::ShadowSunMatrix(sunDir, camPos, camFwd, camRight, camUp, 1.047f, 1.7f, 1.0f, 50.0f, 2048, vp, cn, cf, ce);
  if (!(cn > 0.0f && cf > cn && ce > 0.0f)) {
    return false;
  }
  float corners[8][3];
  Kizuri::ShadowFrustumCorners(camPos, camFwd, camRight, camUp, 1.047f, 1.7f, 1.0f, 50.0f, corners);
  for (int i = 0; i < 8; ++i) {
    float x = corners[i][0];
    float y = corners[i][1];
    float z = corners[i][2];
    float w = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
    if (std::fabs(w) < 1e-6f) {
      return false;
    }
    float nx = (vp[0] * x + vp[4] * y + vp[8] * z + vp[12]) / w;
    float ny = (vp[1] * x + vp[5] * y + vp[9] * z + vp[13]) / w;
    float nz = (vp[2] * x + vp[6] * y + vp[10] * z + vp[14]) / w;
    if (nx < -1.01f || nx > 1.01f || ny < -1.01f || ny > 1.01f || nz < -0.01f || nz > 1.01f) {
      return false;
    }
  }
  return true;
}
bool TestShadowAtlasPlan() {
  int dc[2] = { 3, 2 };
  Kizuri::ShadowAtlasPlan full = Kizuri::PlanShadowAtlas(dc, 2, 2);
  if (full.dirTileStart[0] != 0 || full.dirTileStart[1] != -1) {
    return false;
  }
  if (full.spotMapped != 1 || full.spotTiles[0] != 3) {
    return false;
  }
  int one[1] = { 2 };
  Kizuri::ShadowAtlasPlan part = Kizuri::PlanShadowAtlas(one, 1, 3);
  if (part.dirTileStart[0] != 0) {
    return false;
  }
  if (part.spotMapped != 2 || part.spotTiles[0] != 2 || part.spotTiles[1] != 3) {
    return false;
  }
  Kizuri::ShadowAtlasPlan none = Kizuri::PlanShadowAtlas(nullptr, 0, 5);
  if (none.dirTileStart[0] != -1 || none.spotMapped != 4) {
    return false;
  }
  float u = 0.0f;
  float v = 0.0f;
  Kizuri::ShadowTileUV(-1.0f, 1.0f, 0, 1.0f, u, v);
  if (std::fabs(u) > 1e-5f || std::fabs(v) > 1e-5f) {
    return false;
  }
  Kizuri::ShadowTileUV(1.0f, -1.0f, 3, 1.0f, u, v);
  if (std::fabs(u - 1.0f) > 1e-5f || std::fabs(v - 1.0f) > 1e-5f) {
    return false;
  }
  Kizuri::ShadowTileUV(1.0f, 1.0f, 0, 0.5f, u, v);
  if (std::fabs(u - 0.25f) > 1e-5f || std::fabs(v) > 1e-5f) {
    return false;
  }
  return true;
}
bool TestShadowSpotPoint() {
  float pos[3] = { 0.0f, 8.0f, 0.0f };
  float dir[3] = { 0.0f, -1.0f, 0.0f };
  float vp[16];
  Kizuri::ShadowSpotMatrix(pos, dir, 60.0f, 20.0f, vp);
  float tx = 0.0f;
  float ty = 4.0f;
  float tz = 0.0f;
  float w = vp[3] * tx + vp[7] * ty + vp[11] * tz + vp[15];
  float nx = (vp[0] * tx + vp[4] * ty + vp[8] * tz + vp[12]) / w;
  float ny = (vp[1] * tx + vp[5] * ty + vp[9] * tz + vp[13]) / w;
  float nz = (vp[2] * tx + vp[6] * ty + vp[10] * tz + vp[14]) / w;
  if (std::fabs(nx) > 0.01f || std::fabs(ny) > 0.01f || nz < 0.0f || nz > 1.0f) {
    return false;
  }
  float faces[6][16];
  float ppos[3] = { 5.0f, 3.0f, 5.0f };
  Kizuri::ShadowPointFaces(ppos, 15.0f, faces);
  float dirs[6][3] = {
    { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }
  };
  for (int f = 0; f < 6; ++f) {
    float qx = ppos[0] + dirs[f][0] * 5.0f;
    float qy = ppos[1] + dirs[f][1] * 5.0f;
    float qz = ppos[2] + dirs[f][2] * 5.0f;
    float* m = faces[f];
    float ww = m[3] * qx + m[7] * qy + m[11] * qz + m[15];
    if (std::fabs(ww) < 1e-6f) {
      return false;
    }
    float qnx = (m[0] * qx + m[4] * qy + m[8] * qz + m[12]) / ww;
    float qny = (m[1] * qx + m[5] * qy + m[9] * qz + m[13]) / ww;
    if (std::fabs(qnx) > 0.05f || std::fabs(qny) > 0.05f) {
      return false;
    }
  }
  if (std::fabs(Kizuri::ShadowLinearizeDepth(0.0f, 0.5f, 15.0f) - 0.5f) > 1e-4f) {
    return false;
  }
  if (std::fabs(Kizuri::ShadowLinearizeDepth(1.0f, 0.5f, 15.0f) - 15.0f) > 1e-3f) {
    return false;
  }
  return true;
}
bool TestACES() {
  if (Kizuri::ACESFilm(0.0f) != 0.0f || Kizuri::ACESFilm(-1.0f) != 0.0f) {
    return false;
  }
  float a = Kizuri::ACESFilm(0.18f);
  float b = Kizuri::ACESFilm(1.0f);
  float c = Kizuri::ACESFilm(10.0f);
  if (!(a > 0.0f && b > a && c > b && c <= 1.05f)) {
    return false;
  }
  if (std::fabs(b - 0.8f) > 0.05f) {
    return false;
  }
  return true;
}
bool TestSky() {
  float sunTo[3] = { 0.36f, 0.9f, 0.27f };
  float sunCol[3] = { 1.0f, 0.96f, 0.9f };
  float disk[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(sunTo, sunTo, sunCol, 2.5f, disk);
  float haloDir[3] = { 0.9f, 0.5f, 0.0f };
  float halo[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(haloDir, sunTo, sunCol, 2.5f, halo);
  if (!(disk[0] > halo[0] && disk[1] > halo[1])) {
    return false;
  }
  float up[3] = { 0.0f, 1.0f, 0.0f };
  float upCol[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(up, sunTo, sunCol, 2.5f, upCol);
  float dn[3] = { 0.0f, -1.0f, 0.0f };
  float dnCol[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(dn, sunTo, sunCol, 2.5f, dnCol);
  if (!(upCol[2] > dnCol[2])) {
    return false;
  }
  float up2[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(up, sunTo, sunCol, 2.5f, up2);
  if (upCol[0] != up2[0] || upCol[1] != up2[1] || upCol[2] != up2[2]) {
    return false;
  }
  float setSun[3] = { 0.0f, -1.0f, 0.0f };
  float night[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::SkyGradient(up, setSun, sunCol, 2.5f, night);
  if (!(night[0] < upCol[0] && night[2] < upCol[2])) {
    return false;
  }
  return true;
}
bool TestFog() {
  if (Kizuri::FogTransmittance(0.0f, 100.0f) != 1.0f) {
    return false;
  }
  if (Kizuri::FogTransmittance(0.01f, 0.0f) != 1.0f) {
    return false;
  }
  float t = Kizuri::FogTransmittance(0.01f, 100.0f);
  if (std::fabs(t - 0.36787944f) > 1e-4f) {
    return false;
  }
  float near = Kizuri::FogTransmittance(0.01f, 10.0f);
  if (!(near > t && near < 1.0f)) {
    return false;
  }
  return true;
}
bool TestSSAO() {
  float cam[3] = { 0.0f, 1.5f, -6.0f };
  float p[3] = { 0.0f, 1.5f, 0.0f };
  float front[3] = { 0.0f, 1.5f, -2.0f };
  float kern[3] = { 0.0f, 1.5f, -1.0f };
  float o1 = Kizuri::SsaoTapOcclusion(p, front, kern, cam, 5.0f, 0.01f);
  if (!(o1 > 0.0f && o1 <= 1.0f)) {
    return false;
  }
  float behind[3] = { 0.0f, 1.5f, 2.0f };
  if (Kizuri::SsaoTapOcclusion(p, behind, kern, cam, 5.0f, 0.01f) != 0.0f) {
    return false;
  }
  float far[3] = { 0.0f, 1.5f, -4.9f };
  if (Kizuri::SsaoTapOcclusion(p, far, kern, cam, 1.0f, 0.01f) != 0.0f) {
    return false;
  }
  float o2 = Kizuri::SsaoTapOcclusion(p, front, kern, cam, 5.0f, 0.01f);
  if (o1 != o2) {
    return false;
  }
  return true;
}
bool TestPCSSMath() {
  if (Kizuri::PCSSPenumbraWidth(10.0f, 10.0f, 0.5f) != 0.0f) {
    return false;
  }
  if (Kizuri::PCSSPenumbraWidth(10.0f, 5.0f, 0.0f) != 0.0f) {
    return false;
  }
  if (Kizuri::PCSSPenumbraWidth(5.0f, 10.0f, 0.5f) != 0.0f) {
    return false;
  }
  if (std::fabs(Kizuri::PCSSPenumbraWidth(10.0f, 5.0f, 0.5f) - 0.5f) > 1e-5f) {
    return false;
  }
  if (std::fabs(Kizuri::PCSSOrthoUVScale(20.0f, 1.0f) - 0.0125f) > 1e-6f) {
    return false;
  }
  if (Kizuri::PCSSOrthoUVScale(0.0f, 1.0f) != 0.0f) {
    return false;
  }
  float puv = Kizuri::PCSSPerspUVScale(0.5f, 10.0f, 1.0f);
  if (std::fabs(puv - 0.05f) > 1e-6f) {
    return false;
  }
  return true;
}
bool TestAddRemoveMeshComponent() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  if (scene.Get(a)->hasMesh) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> add(new Kizuri::AddMeshCmd(a));
  if (!undo.Execute(std::move(add), scene) || !scene.Get(a)->hasMesh) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> addAgain(new Kizuri::AddMeshCmd(a));
  if (undo.Execute(std::move(addAgain), scene)) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> set(new Kizuri::SetMeshGuidCmd(a, "", "mesh-9"));
  if (!undo.Execute(std::move(set), scene)) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> rem(new Kizuri::RemoveMeshCmd(a));
  if (!undo.Execute(std::move(rem), scene)) {
    return false;
  }
  if (scene.Get(a)->hasMesh || !scene.Get(a)->meshGuid.empty()) {
    return false;
  }
  if (!undo.Undo(scene) || !scene.Get(a)->hasMesh || scene.Get(a)->meshGuid != "mesh-9") {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->hasMesh) {
    return false;
  }
  return true;
}
bool TestSetMeshGuid() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::SetMeshGuidCmd(a, "", "mesh-1"));
  if (!undo.Execute(std::move(cmd), scene) || scene.Get(a)->meshGuid != "mesh-1") {
    return false;
  }
  if (!scene.Get(a)->hasMesh) {
    return false;
  }
  if (!scene.IsDirty()) {
    return false;
  }
  if (!undo.Undo(scene) || !scene.Get(a)->meshGuid.empty()) {
    return false;
  }
  if (scene.Get(a)->hasMesh) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->meshGuid != "mesh-1") {
    return false;
  }
  std::unique_ptr<Kizuri::Command> bad(new Kizuri::SetMeshGuidCmd(Kizuri::EntityId::Invalid(), "", "x"));
  if (undo.Execute(std::move(bad), scene)) {
    return false;
  }
  Kizuri::EntityId b = scene.CreateEntity("B");
  scene.Get(b)->meshGuid = "mesh-2";
  scene.Get(b)->hasMesh = true;
  std::unique_ptr<Kizuri::Command> del(new Kizuri::DeleteEntityCmd(b));
  if (!undo.Execute(std::move(del), scene)) {
    return false;
  }
  if (!undo.Undo(scene)) {
    return false;
  }
  const Kizuri::Entity* rb = nullptr;
  std::vector<Kizuri::EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* e = scene.Get(all[i]);
    if (e->name == "B") {
      rb = e;
    }
  }
  return rb != nullptr && rb->meshGuid == "mesh-2";
}
bool TestDbTextures() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzdbtex_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  WriteTestBMP((dir / "wall.bmp").string(), 16, 16, false);
  Kizuri::AssetDatabase db;
  db.SetAssetsDir(dir.string());
  db.Scan();
  db.DrainBlocking();
  if (db.AllTexGuids().size() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::string tguid = db.AllTexGuids()[0];
  const Kizuri::TextureRecord* tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || !tr->loaded || tr->state != Kizuri::TextureAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (tr->data.width != 16 || tr->data.mips.size() < 2 || tr->data.format != Kizuri::TexFormat::Bc1) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  if (db.PendingImports() != 0) {
    fs::remove_all(dir, ec);
    return false;
  }
  {
    FILE* fp = std::fopen((dir / "wall.bmp").string().c_str(), "ab");
    std::fputs(" ", fp);
    std::fclose(fp);
  }
  fs::copy_file(dir / "wall.bmp", fs::temp_directory_path() / "kzdbtex_backup.bmp", ec);
  db.Scan();
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::Outdated) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.ReimportTexture(tguid)) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::Ready || db.AllTexGuids()[0] != tguid) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::create_directories(dir / "Sub", ec);
  fs::copy_file(dir / "wall.bmp", dir / "Sub" / "moved.bmp", ec);
  fs::remove(dir / "wall.bmp", ec);
  db.Scan();
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::Ready || tr->sourcePath.find("moved.bmp") == std::string::npos) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(dir / "Sub" / "moved.bmp", ec);
  db.Scan();
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::SourceMissing) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::copy_file(fs::temp_directory_path() / "kzdbtex_backup.bmp", dir / "Sub" / "moved2.bmp", ec);
  if (ec) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.SetTexSourcePath(tguid, (dir / "Sub" / "moved2.bmp").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (db.SetTexSourcePath(tguid, (dir / "nope.bmp").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove(dir / "Sub" / "moved2.bmp", ec);
  db.Scan();
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::SourceMissing) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::copy_file(fs::temp_directory_path() / "kzdbtex_backup.bmp", dir / "Sub" / "moved.bmp", ec);
  if (db.RelocateMissing() != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.DrainBlocking();
  tr = db.GetTexByGuid(tguid);
  if (tr == nullptr || tr->state != Kizuri::TextureAssetState::Ready) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshAssetData mesh;
  mesh.guid = "texuser-mesh-1";
  mesh.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
  mesh.normals = { 0, 0, 1, 0, 0, 1, 0, 0, 1 };
  mesh.uvs = { 0, 0, 1, 0, 0, 1 };
  mesh.indices = { 0, 1, 2 };
  if (!Kizuri::GenerateTangents(mesh.positions, mesh.normals, mesh.uvs, mesh.indices, mesh.tangents)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshMaterialData mat;
  mat.name = "M";
  mat.albedo[0] = 1.0f;
  mat.albedo[1] = 1.0f;
  mat.albedo[2] = 1.0f;
  mat.metallic = 0.0f;
  mat.roughness = 0.5f;
  mat.albedoTexGuid = tguid;
  mesh.materials.push_back(mat);
  Kizuri::MeshPartData part;
  part.indexOffset = 0;
  part.indexCount = 3;
  part.material = 0;
  mesh.parts.push_back(part);
  mesh.aabbMin[0] = 0.0f;
  mesh.aabbMin[1] = 0.0f;
  mesh.aabbMin[2] = 0.0f;
  mesh.aabbMax[0] = 1.0f;
  mesh.aabbMax[1] = 1.0f;
  mesh.aabbMax[2] = 0.0f;
  mesh.hasSource = false;
  if (!Kizuri::EncodeMeshFile(mesh, (dir / "user.kzmesh").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  db.Scan();
  db.DrainBlocking();
  std::map<std::string, size_t> counts = db.ComputeTexRefCounts();
  if (counts[tguid] != 1) {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<std::string> blocked;
  if (db.DeleteTextureFile(tguid, false, blocked)) {
    fs::remove_all(dir, ec);
    return false;
  }
  if (blocked.size() != 1 || blocked[0] != "user.kzmesh") {
    fs::remove_all(dir, ec);
    return false;
  }
  std::vector<std::string> blocked2;
  if (!db.DeleteTextureFile(tguid, true, blocked2) || db.GetTexByGuid(tguid) != nullptr) {
    fs::remove_all(dir, ec);
    return false;
  }
  fs::remove_all(dir, ec);
  fs::remove(fs::temp_directory_path() / "kzdbtex_backup.bmp", ec);
  return true;
}
bool TestRHIBuffers() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 64;
  desc.height = 64;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->CreateBufferEmpty(0, 16, false) != 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::RHIBuffer buf = rhi->CreateBufferEmpty(1024, 16, false);
  if (buf == 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  std::vector<unsigned char> chunk(256, 0xAB);
  if (!rhi->UpdateBufferRange(buf, 0, chunk.data(), chunk.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->UpdateBufferRange(buf, 900, chunk.data(), chunk.size())) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->UpdateBufferRange(buf, 0, nullptr, 10)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (!rhi->UpdateBufferRange(buf, 768, chunk.data(), 256)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->DestroyBuffer(buf);
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
}
bool TestMeshStaging() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "kzstaging_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  Kizuri::MeshAssetData big;
  big.guid = Kizuri::GenerateGuidString();
  const size_t nv = 20000;
  for (size_t i = 0; i < nv; ++i) {
    big.positions.push_back(static_cast<float>(i) * 0.01f);
    big.positions.push_back(0.0f);
    big.positions.push_back(0.0f);
    big.normals.push_back(0.0f);
    big.normals.push_back(1.0f);
    big.normals.push_back(0.0f);
    big.uvs.push_back(0.0f);
    big.uvs.push_back(0.0f);
  }
  for (uint32_t i = 0; i + 2 < nv; i += 3) {
    big.indices.push_back(i);
    big.indices.push_back(i + 1);
    big.indices.push_back(i + 2);
  }
  if (!Kizuri::GenerateTangents(big.positions, big.normals, big.uvs, big.indices, big.tangents)) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshPartData part;
  part.indexOffset = 0;
  part.indexCount = static_cast<uint32_t>(big.indices.size());
  part.material = 0;
  big.parts.push_back(part);
  big.aabbMin[0] = 0.0f;
  big.aabbMin[1] = 0.0f;
  big.aabbMin[2] = 0.0f;
  big.aabbMax[0] = 200.0f;
  big.aabbMax[1] = 0.0f;
  big.aabbMax[2] = 0.0f;
  big.hasSource = false;
  if (!Kizuri::EncodeMeshFile(big, (dir / "big.kzmesh").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::MeshAssetData small;
  small.guid = Kizuri::GenerateGuidString();
  small.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
  small.normals = { 0, 0, 1, 0, 0, 1, 0, 0, 1 };
  small.uvs = { 0, 0, 1, 0, 0, 1 };
  small.indices = { 0, 1, 2 };
  if (!Kizuri::GenerateTangents(small.positions, small.normals, small.uvs, small.indices, small.tangents)) {
    fs::remove_all(dir, ec);
    return false;
  }
  part.indexCount = 3;
  small.parts.push_back(part);
  small.aabbMin[0] = 0.0f;
  small.aabbMin[1] = 0.0f;
  small.aabbMin[2] = 0.0f;
  small.aabbMax[0] = 1.0f;
  small.aabbMax[1] = 1.0f;
  small.aabbMax[2] = 0.0f;
  small.hasSource = false;
  if (!Kizuri::EncodeMeshFile(small, (dir / "small.kzmesh").string())) {
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::AssetDatabase db;
  db.SetAssetsDir(dir.string());
  db.Scan();
  db.DrainBlocking();
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 64;
  desc.height = 64;
  desc.vsync = false;
  rhi->Initialize(desc);
  db.SetGpuRHI(rhi);
  if (db.EnsureMeshGpu(big.guid, 0)) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  if (!db.EnsureMeshGpu(small.guid, 1048576)) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  int iters = 0;
  while (!db.EnsureMeshGpu(big.guid, 4096)) {
    if (++iters > 10000) {
      Kizuri::DestroyRHI(rhi);
      fs::remove_all(dir, ec);
      return false;
    }
  }
  if (iters < 2) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  const Kizuri::MeshRecord* rec = db.GetByGuid(big.guid);
  if (rec == nullptr || !rec->gpuReady || rec->gpuCount != big.indices.size() || rec->gpuVB == 0 || rec->gpuIB == 0) {
    Kizuri::DestroyRHI(rhi);
    fs::remove_all(dir, ec);
    return false;
  }
  Kizuri::DestroyRHI(rhi);
  fs::remove_all(dir, ec);
  return true;
}
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("KizuriHello %s\n", Kizuri::Core_Version());
  int failures = 0;
  failures += Check("DirectXMath", Kizuri::Core_TestDirectXMath());
  failures += Check("enkiTS", Kizuri::Core_TestTaskSystem());
  failures += Check("zstd", Kizuri::Core_TestZstd());
  failures += Check("FlatBuffers", Kizuri::Core_TestFlatBuffers());
  failures += Check("DearImGui", Kizuri::Render_TestImGui());
  failures += Check("ImGuizmo", Kizuri::Render_TestGizmo());
  failures += Check("cgltf", Kizuri::Render_TestCgltf());
  failures += Check("stb_image", Kizuri::Render_TestStb());
  failures += Check("miniaudio-noDevice", Kizuri::Audio_TestEngineNoDevice());
  failures += Check("Physics", Kizuri::Physics_SelfTest());
  failures += Check("Animation", Kizuri::Animation_SelfTest());
  failures += Check("Scripting", Kizuri::Scripting_SelfTest());
  failures += Check("PoolAllocator", TestPool());
  failures += Check("ArenaAllocator", TestArena());
  failures += Check("JobSystem", TestJobSystem());
  failures += Check("OriginRebasing", TestOrigin());
  failures += Check("RHI-Null", TestRHINull());
  failures += Check("RHI-StateCache", TestStateCache());
  failures += Check("FreeCamera", TestCamera());
  failures += Check("MeshLoader-gltf", TestMeshLoader());
  failures += Check("PBR-Directional", TestPBR());
  failures += Check("ShaderFiles", TestShaderFiles());
  failures += Check("Deferred-Null", TestDeferredNull());
  failures += Check("Scene-CRUD", TestSceneCrud());
  failures += Check("Scene-Duplicate", TestSceneDuplicate());
  failures += Check("Scene-Parent", TestSceneParent());
  failures += Check("Selection-Set", TestSelectionSet());
  failures += Check("ScreenRect", TestScreenRect());
  failures += Check("ScreenRectBehind", TestScreenRectBehind());
  failures += Check("RectOverlap", TestRectOverlap());
  failures += Check("LogStore", TestLog());
  failures += Check("EditQueue", TestEditQueue());
  failures += Check("Picking", TestPicking());
  failures += Check("Scene-RoundTrip", TestSceneRoundTrip());
  failures += Check("Scene-InvalidFile", TestSceneInvalid());
  failures += Check("Undo-Create", TestUndoCreate());
  failures += Check("Undo-Delete", TestUndoDelete());
  failures += Check("Undo-Edit", TestUndoEdit());
  failures += Check("Undo-RenameParent", TestUndoRenameParent());
  failures += Check("Undo-Stack", TestUndoStack());
  failures += Check("Undo-MultiMove", TestUndoMultiMove());
  failures += Check("Undo-MultiPartial", TestUndoMultiPartial());
  failures += Check("Autosave-Paths", TestAutosavePaths());
  failures += Check("Autosave-Update", TestAutosaveUpdate());
  failures += Check("Autosave-Offer", TestAutosaveOffer());
  failures += Check("Notifications", TestNotifications());
  failures += Check("Guid", TestGuid());
  failures += Check("TangentGen", TestTangentGen());
  failures += Check("MeshCodec", TestMeshCodec());
  failures += Check("MeshImport", TestMeshImport());
  failures += Check("AssetDatabase", TestAssetDatabase());
  failures += Check("SceneMeshGuid", TestSceneMeshGuid());
  failures += Check("ImportNoRetry", TestImportNoRetry());
  failures += Check("TexCodec", TestTexCodec());
  failures += Check("TexImport", TestTexImport());
  failures += Check("TexImportOpaque", TestTexImportOpaque());
  failures += Check("TexImportAlpha", TestTexImportAlpha());
  failures += Check("TexImportNormal", TestTexImportNormal());
  failures += Check("TexImportOrm", TestTexImportOrm());
  failures += Check("EnsureTextureKind", TestEnsureTextureKind());
  failures += Check("TexImportOrm", TestTexImportOrm());
  failures += Check("EnsureTextureKind", TestEnsureTextureKind());
  failures += Check("RHITextures", TestRHITextures());
  failures += Check("TexStreaming", TestTexStreaming());
  failures += Check("GltfTextured", TestGltfTextured());
  failures += Check("RHIBuffers", TestRHIBuffers());
  failures += Check("MeshStaging", TestMeshStaging());
  failures += Check("SetMeshGuid", TestSetMeshGuid());
  failures += Check("AddRemoveMesh", TestAddRemoveMeshComponent());
  failures += Check("LightDefaults", TestLightDefaults());
  failures += Check("LightTypeRetention", TestLightTypeRetention());
  failures += Check("LightRoundTrip", TestLightRoundTrip());
  failures += Check("LightUndo", TestLightUndo());
  failures += Check("MaterialDefaults", TestMaterialDefaults());
  failures += Check("MaterialRoundTrip", TestMaterialRoundTrip());
  failures += Check("MaterialUndo", TestMaterialUndo());
  failures += Check("EntityForward", TestEntityForward());
  failures += Check("PBR-PointSpot", TestPBRPointSpot());
  failures += Check("LightGizmo", TestLightGizmo());
  failures += Check("ShaderCompile", TestShaderCompile());
  failures += Check("ShadowSplits", TestShadowSplits());
  failures += Check("ShadowSunMatrix", TestShadowSunMatrix());
  failures += Check("ShadowAtlasPlan", TestShadowAtlasPlan());
  failures += Check("ShadowSpotPoint", TestShadowSpotPoint());
  failures += Check("PCSSMath", TestPCSSMath());
  failures += Check("ACES", TestACES());
  failures += Check("Sky", TestSky());
  failures += Check("SSAO", TestSSAO());
  failures += Check("Fog", TestFog());
  failures += Check("DbTextures", TestDbTextures());
  if (failures == 0) {
    std::printf("KizuriHello: all bootstrap libs linked and functional\n");
  } else {
    std::printf("KizuriHello: %d failures\n", failures);
  }
  return failures == 0 ? 0 : 1;
}
