#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Input.h"
#include "Kizuri/SceneSerializer.h"
#include "Kizuri/Assets/MeshCodec.h"
#include "FileDialog.h"
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstdio>
#include <filesystem>
#include <shellapi.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace Kizuri {
EditorApp::EditorApp()
  : rhi(nullptr)
  , cubeReady(false)
  , running(false)
  , showHierarchy(true)
  , showInspector(true)
  , showConsole(true)
  , showViewport(true)
  , showAssetBrowser(true)
  , showAbout(false)
  , showSettings(false)
  , debugCascades(false)
  , viewExposure(1.0f)
  , viewACES(true)
  , viewToneMap(0)
  , viewAutoExp(false)
  , viewContrast(1.0f)
  , viewSaturation(1.0f)
  , viewTemp(0.0f)
  , viewTint(0.0f)
  , viewLutAmount(0.0f)
  , viewBloom(0.5f)
  , viewScale(1.0f)
  , viewSharp(0.5f)
  , viewDynRes(false)
  , viewDynTarget(16.6f)
  , viewOcc(false)
  , viewShadowAuto(false)
  , viewSsao(true)
  , viewSsaoIntensity(1.0f)
  , viewSsaoRadius(0.5f)
  , viewSsr(true)
  , viewContact(true)
  , viewSsgi(true)
  , viewFxaa(true)
  , viewTaa(false)
  , viewEnv(1.0f)
  , viewDof(false)
  , viewFocusDist(10.0f)
  , viewFocusRange(20.0f)
  , viewMotion(false)
  , viewMotionScale(1.0f)
  , viewCa(0.0f)
  , viewVig(0.15f)
  , viewGrain(0.04f)
  , viewFog(true)
  , viewFogDensity(0.002f)
  , viewQuality(2)
  , viewX(0.0f)
  , viewY(0.0f)
  , viewW(1280.0f)
  , viewH(720.0f)
  , viewValid(false)
  , frameDt(0.016f)
  , lastMouseX(0)
  , lastMouseY(0)
  , downPosValid(false)
  , downX(0)
  , downY(0)
  , rdownX(0)
  , rdownY(0)
  , rdownValid(false)
  , contextPick(EntityId::Invalid())
  , gizmoHotLast(false)
  , gizmoDragging(false)
  , renameActive(false)
  , renameTarget(EntityId::Invalid())
  , titleDirtyShown(false)
  , pendingAction(0)
  , openDialogQueued(false)
  , saveDialogQueued(false)
  , afterSaveRunPending(false)
  , savePromptQueued(false)
  , restorePromptQueued(false) {
  viewFogColor[0] = 0.6f;
  viewFogColor[1] = 0.7f;
  viewFogColor[2] = 0.8f;
  renameBuf[0] = '\0';
  showNotifHistory = false;
  assetRenameBuf[0] = '\0';
  assetRenameActive = false;
  assetRenameIsTex = false;
  selectedAssetIsTex = false;
  forceDeleteIsTex = false;
  defaultVB = 0;
  defaultIB = 0;
  defaultCount = 0;
  scanTimer = 0.0;
  MakeIdentityTransform(gizmoStart);
  gizmoTarget = EntityId::Invalid();
  gizmoJustEnded = false;
  rubberActive = false;
  rubberX0 = 0.0f;
  rubberY0 = 0.0f;
}
std::string EditorApp::FindShaderDir() {
  const char* dirs[4] = { "Shaders", "../Shaders", "../../Shaders", "build/bin/Release/Shaders" };
  for (int i = 0; i < 4; ++i) {
    std::string p = std::string(dirs[i]) + "/GeometryVS.hlsl";
    FILE* fp = std::fopen(p.c_str(), "rb");
    if (fp != nullptr) {
      std::fclose(fp);
      return dirs[i];
    }
  }
  return "Shaders";
}
std::string EditorApp::ResolveAssetsDir() {
  const char* dirs[4] = { "Assets", "Samples/Assets", "../Samples/Assets", "../../Samples/Assets" };
  for (int i = 0; i < 4; ++i) {
    std::error_code ec;
    if (std::filesystem::is_directory(dirs[i], ec) && !ec) {
      return std::filesystem::absolute(dirs[i], ec).string();
    }
  }
  return std::filesystem::absolute("Assets").string();
}
std::vector<RefUse> EditorApp::SceneMeshRefs() {
  std::vector<RefUse> refs;
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e != nullptr && e->hasMesh && !e->meshGuid.empty()) {
      RefUse r;
      r.userLabel = e->name;
      r.assetGuid = e->meshGuid;
      refs.push_back(r);
    }
  }
  return refs;
}
void EditorApp::UploadDefaultCube() {
  if (!cubeReady || defaultVB != 0) {
    return;
  }
  size_t vertexCount = cubeMesh.positions.size() / 3;
  if (vertexCount == 0 || cubeMesh.indices.empty()) {
    return;
  }
  std::vector<float> interleaved;
  interleaved.reserve(vertexCount * 14);
  for (size_t i = 0; i < vertexCount; ++i) {
    interleaved.push_back(cubeMesh.positions[i * 3 + 0]);
    interleaved.push_back(cubeMesh.positions[i * 3 + 1]);
    interleaved.push_back(cubeMesh.positions[i * 3 + 2]);
    interleaved.push_back(cubeMesh.normals[i * 3 + 0]);
    interleaved.push_back(cubeMesh.normals[i * 3 + 1]);
    interleaved.push_back(cubeMesh.normals[i * 3 + 2]);
    interleaved.push_back(cubeMesh.uvs[i * 2 + 0]);
    interleaved.push_back(cubeMesh.uvs[i * 2 + 1]);
    if (cubeMesh.tangents.size() == vertexCount * 4) {
      interleaved.push_back(cubeMesh.tangents[i * 4 + 0]);
      interleaved.push_back(cubeMesh.tangents[i * 4 + 1]);
      interleaved.push_back(cubeMesh.tangents[i * 4 + 2]);
      interleaved.push_back(cubeMesh.tangents[i * 4 + 3]);
    } else {
      interleaved.push_back(1.0f);
      interleaved.push_back(0.0f);
      interleaved.push_back(0.0f);
      interleaved.push_back(1.0f);
    }
    if (cubeMesh.uv2s.size() == vertexCount * 2) {
      interleaved.push_back(cubeMesh.uv2s[i * 2 + 0]);
      interleaved.push_back(cubeMesh.uv2s[i * 2 + 1]);
    } else {
      interleaved.push_back(0.0f);
      interleaved.push_back(0.0f);
    }
  }
  defaultVB = rhi->CreateBuffer(static_cast<uint64_t>(interleaved.size() * sizeof(float)), 56, false, interleaved.data());
  defaultIB = rhi->CreateBuffer(static_cast<uint64_t>(cubeMesh.indices.size() * 4), 4, true, cubeMesh.indices.data());
  if (defaultVB == 0 || defaultIB == 0) {
    if (defaultVB != 0) {
      rhi->DestroyBuffer(defaultVB);
      defaultVB = 0;
    }
    if (defaultIB != 0) {
      rhi->DestroyBuffer(defaultIB);
      defaultIB = 0;
    }
    return;
  }
  defaultCount = static_cast<uint32_t>(cubeMesh.indices.size());
}
void EditorApp::PumpAssets() {
  scanTimer += frameDt;
  if (scanTimer >= 2.0) {
    scanTimer = 0.0;
    assets.Scan();
    assets.RelocateMissing();
  }
  assets.DrainCompleted();
  {
    std::vector<std::string> errors = assets.TakeImportErrors();
    for (size_t i = 0; i < errors.size(); ++i) {
      Announce(LogLevel::Error, std::string("Import failed: ") + errors[i]);
    }
  }
  {
    std::vector<std::string> warns = assets.TakeImportWarnings();
    for (size_t i = 0; i < warns.size(); ++i) {
      Announce(LogLevel::Warning, std::string("Import warning: ") + warns[i]);
    }
  }
  {
    std::vector<std::string> names = assets.TakeRelocated();
    if (!names.empty()) {
      std::string msg = "Source relocated: ";
      for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
          msg += ", ";
        }
        msg += names[i];
      }
      Announce(LogLevel::Info, msg);
    }
  }
  float cpx = 0.0f;
  float cpy = 0.0f;
  float cpz = 0.0f;
  camera.GetPosition(cpx, cpy, cpz);
  float camPos[3] = { cpx, cpy, cpz };
  std::vector<EntityId> all = scene.All();
  std::vector<MeshUse> uses;
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr || ((!e->hasMesh || e->meshGuid.empty()) && !e->hasDecal)) {
      continue;
    }
    if (e->hasLod && !e->lod.farGuid.empty() && e->lod.farGuid != e->meshGuid) {
      MeshUse lu;
      lu.meshGuid = e->lod.farGuid;
      lu.pos[0] = e->transform.position[0];
      lu.pos[1] = e->transform.position[1];
      lu.pos[2] = e->transform.position[2];
      uses.push_back(lu);
    }
    MeshUse u;
    u.meshGuid = e->meshGuid;
    u.pos[0] = e->transform.position[0];
    u.pos[1] = e->transform.position[1];
    u.pos[2] = e->transform.position[2];
    if (e->hasMesh && !e->meshGuid.empty()) {
      uses.push_back(u);
    }
    float dx = u.pos[0] - camPos[0];
    float dy = u.pos[1] - camPos[1];
    float dz = u.pos[2] - camPos[2];
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (e->hasMaterial) {
      assets.RequestTexture(e->material.albedoTex, dist);
      assets.RequestTexture(e->material.normalTex, dist);
      assets.RequestTexture(e->material.ormTex, dist);
      assets.RequestTexture(e->material.heightTex, dist);
      assets.RequestTexture(e->material.emissiveTex, dist);
      assets.RequestTexture(e->material.specularTex, dist);
      assets.RequestTexture(e->material.specularColorTex, dist);
      assets.RequestTexture(e->material.clearcoatTex, dist);
      assets.RequestTexture(e->material.clearcoatRoughnessTex, dist);
      assets.RequestTexture(e->material.clearcoatNormalTex, dist);
      assets.RequestTexture(e->material.sheenColorTex, dist);
      assets.RequestTexture(e->material.sheenRoughnessTex, dist);
      assets.RequestTexture(e->material.anisotropyTex, dist);
      assets.RequestTexture(e->material.iridescenceTex, dist);
      assets.RequestTexture(e->material.iridescenceThicknessTex, dist);
      assets.RequestTexture(e->material.transmissionTex, dist);
      assets.RequestTexture(e->material.thicknessTex, dist);
    }
    if (e->hasDecal) {
      assets.RequestTexture(e->decal.texGuid, dist);
    }
    if (e->hasLightmap) {
      assets.RequestTexture(e->lightmap.texGuid, dist);
    }
  }
  assets.UpdateStreaming(camPos, uses);
  assets.DrainUploads(rhi, 262144);
}
void EditorApp::HandleOsDrop(void* hdrop) {
  HDROP drop = static_cast<HDROP>(hdrop);
  UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
  for (UINT i = 0; i < count; ++i) {
    wchar_t file[1024];
    if (DragQueryFileW(drop, i, file, 1024) == 0) {
      continue;
    }
    char narrow[1024];
    int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, narrow, 1024, nullptr, nullptr);
    if (n <= 1) {
      continue;
    }
    std::string src(narrow);
    std::string ext;
    size_t dot = src.find_last_of('.');
    if (dot != std::string::npos) {
      ext = src.substr(dot);
      for (size_t k = 0; k < ext.size(); ++k) {
        ext[k] = static_cast<char>(tolower(ext[k]));
      }
    }
    if (ext != ".glb" && ext != ".gltf" && ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga" && ext != ".bmp" && ext != ".kzmesh" && ext != ".kztex") {
      Announce(LogLevel::Warning, std::string("Dropped file ignored: ") + src);
      continue;
    }
    size_t slash = src.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? src : src.substr(slash + 1);
    std::string dst = assets.AssetsDir() + "/" + name;
    int suffix = 1;
    while (std::filesystem::exists(dst)) {
      size_t d = name.find_last_of('.');
      std::string stem = (d == std::string::npos) ? name : name.substr(0, d);
      std::string dext = (d == std::string::npos) ? "" : name.substr(d);
      dst = assets.AssetsDir() + "/" + stem + "_" + std::to_string(suffix++) + dext;
    }
    if (!CopyFileW(file, std::filesystem::path(dst).wstring().c_str(), TRUE)) {
      Announce(LogLevel::Error, std::string("Drop copy failed: ") + name);
      continue;
    }
    Announce(LogLevel::Success, std::string("Dropped into Assets: ") + name);
  }
  DragFinish(drop);
}
std::string EditorApp::FindAsset(const char* name) {
  const char* dirs[4] = { "Assets", "Samples/Assets", "../Assets", "build/bin/Release/Assets" };
  for (int i = 0; i < 4; ++i) {
    std::string p = std::string(dirs[i]) + "/" + name;
    FILE* fp = std::fopen(p.c_str(), "rb");
    if (fp != nullptr) {
      std::fclose(fp);
      return p;
    }
  }
  return name;
}
bool EditorApp::Initialize() {
  if (!window.Create(L"Kizuri Editor - Fase 3", 1600, 900)) {
    return false;
  }
  window.SetMessageHook([this](void* hwnd, unsigned int msg, unsigned long long wParam, long long lParam) -> long long {
    if (msg == WM_DROPFILES) {
      HandleOsDrop(reinterpret_cast<void*>(static_cast<uintptr_t>(lParam)));
      return 1;
    }
    LRESULT r = ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
    return r != 0 ? 1 : 0;
  });
  DragAcceptFiles(static_cast<HWND>(window.NativeHandle()), TRUE);
  RawInputPoll::Initialize(window.NativeHandle());
  RHIDesc desc;
  desc.windowHandle = window.NativeHandle();
  desc.width = window.Width();
  desc.height = window.Height();
  desc.vsync = true;
  rhi = CreateRHI(RHI_API::D3D11);
  if (rhi == nullptr || !rhi->Initialize(desc)) {
    return false;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  ImGui_ImplWin32_Init(window.NativeHandle());
  ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(rhi->GetNativeDevice()), static_cast<ID3D11DeviceContext*>(rhi->GetNativeContext()));
  std::string cubePath = FindAsset("cube.gltf");
  if (LoadStaticMeshFromGltf(cubePath.c_str(), cubeMesh)) {
    if (!Kizuri::GenerateTangents(cubeMesh.positions, cubeMesh.normals, cubeMesh.uvs, cubeMesh.indices, cubeMesh.tangents)) {
      Announce(LogLevel::Error, std::string("Cube tangents failed: ") + cubePath);
    }
    cubeMesh.uv2s.assign(cubeMesh.positions.size() / 3 * 2, 0.0f);
    cubeReady = true;
    log.Add(LogLevel::Info, std::string("Mesh loaded: ") + cubePath);
  } else {
    Announce(LogLevel::Error, std::string("Mesh load failed: ") + cubePath);
  }
  std::string shaderDir = FindShaderDir();
  if (!renderer.Initialize(rhi, window.Width(), window.Height(), shaderDir.c_str())) {
    log.Add(LogLevel::Error, "Deferred renderer init failed");
    return false;
  }
  if (cubeReady) {
    const float* cubeTan = cubeMesh.tangents.size() == cubeMesh.positions.size() / 3 * 4 ? cubeMesh.tangents.data() : nullptr;
    const float* cubeUv2 = cubeMesh.uv2s.size() == cubeMesh.positions.size() / 3 * 2 ? cubeMesh.uv2s.data() : nullptr;
    renderer.SetMesh(cubeMesh.positions.data(), cubeMesh.normals.data(), cubeMesh.uvs.data(), cubeMesh.positions.size() / 3, cubeMesh.indices.data(), cubeMesh.indices.size(), cubeTan, cubeUv2);
  }
  DeferredMaterial mat;
  mat.albedo[0] = 0.75f;
  mat.albedo[1] = 0.22f;
  mat.albedo[2] = 0.12f;
  mat.roughness = 0.45f;
  mat.metallic = 0.05f;
  renderer.SetMaterial(mat);
  camera.SetPosition(0.0f, 1.5f, -6.0f);
  int mx = 0;
  int my = 0;
  RawInputPoll::GetMousePosition(mx, my);
  lastMouseX = mx;
  lastMouseY = my;
  log.Add(LogLevel::Success, "Kizuri Editor ready");
  gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
  InitStoragePaths();
  assets.SetAssetsDir(ResolveAssetsDir());
  assets.SetGpuRHI(rhi);
  assets.Scan();
  UploadDefaultCube();
  OfferRestoreFor(ReadLastScene());
  running = true;
  return true;
}
void EditorApp::Shutdown() {
  if (!scene.IsDirty()) {
    DeleteRecoveryFile(CurrentRecoveryPath());
  }
  if (rhi != nullptr) {
    if (defaultVB != 0) {
      rhi->DestroyBuffer(defaultVB);
      defaultVB = 0;
    }
    if (defaultIB != 0) {
      rhi->DestroyBuffer(defaultIB);
      defaultIB = 0;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    renderer.Shutdown();
    DestroyRHI(rhi);
    rhi = nullptr;
  }
  RawInputPoll::Shutdown();
  window.ClearMessageHook();
  window.Destroy();
}
int EditorApp::Run() {
  unsigned long long prev = GetTickCount64();
  while (running && window.PollEvents()) {
    if (window.IsMinimized()) {
      Sleep(10);
      prev = GetTickCount64();
      continue;
    }
    unsigned long long now = GetTickCount64();
    float dt = static_cast<float>(now - prev) / 1000.0f;
    prev = now;
    if (dt > 0.1f) {
      dt = 0.1f;
    }
    edits.ApplyAll(scene, undo);
    if (autosave.Update(dt, scene, CurrentRecoveryPath())) {
      log.Add(LogLevel::Info, "Autosaved");
    }
    notifications.Update(dt);
    PumpAssets();
    RawInputPoll::Poll();
    frameDt = dt;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    Frame();
    RenderScene();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    rhi->Present(true);
  }
  return 0;
}
void EditorApp::UpdateCamera(float dt, bool lookNow) {
  if (!viewValid) {
    return;
  }
  ImGuiIO& io = ImGui::GetIO();
  int mx = 0;
  int my = 0;
  RawInputPoll::GetMousePosition(mx, my);
  int mdx = mx - lastMouseX;
  int mdy = my - lastMouseY;
  lastMouseX = mx;
  lastMouseY = my;
  if (io.WantTextInput) {
    return;
  }
  camera.Update(
    dt,
    lookNow && RawInputPoll::IsKeyDown(0x57),
    lookNow && RawInputPoll::IsKeyDown(0x53),
    lookNow && RawInputPoll::IsKeyDown(0x41),
    lookNow && RawInputPoll::IsKeyDown(0x44),
    lookNow && RawInputPoll::IsKeyDown(0x45),
    lookNow && RawInputPoll::IsKeyDown(0x51),
    lookNow ? static_cast<float>(mdx) : 0.0f,
    lookNow ? static_cast<float>(mdy) : 0.0f);
}
void EditorApp::CreateEntityAt(float x, float y, float z, const std::string& meshGuid) {
  Transform t;
  MakeIdentityTransform(t);
  t.position[0] = x;
  t.position[1] = y;
  t.position[2] = z;
  std::unique_ptr<Command> cmd(new CreateEntityCmd("Entity", t, EntityId::Invalid(), meshGuid));
  std::vector<EntityId> beforeIds = scene.All();
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  SelectNewEntity(beforeIds);
  log.Add(LogLevel::Info, "Created Entity");
}
void EditorApp::DoUndo() {
  if (!undo.CanUndo()) {
    return;
  }
  std::string name = undo.UndoName();
  undo.Undo(scene);
  SyncSelection();
  log.Add(LogLevel::Info, std::string("Undo ") + name);
}
void EditorApp::DoRedo() {
  if (!undo.CanRedo()) {
    return;
  }
  std::string name = undo.RedoName();
  undo.Redo(scene);
  SyncSelection();
  log.Add(LogLevel::Info, std::string("Redo ") + name);
}
void EditorApp::SyncSelection() {
  std::vector<EntityId> all = selection.All();
  for (size_t i = 0; i < all.size(); ++i) {
    if (!scene.Has(all[i])) {
      selection.Remove(all[i]);
    }
  }
}
void EditorApp::SelectNewEntity(const std::vector<EntityId>& beforeIds) {
  std::vector<EntityId> afterIds = scene.All();
  for (size_t i = 0; i < afterIds.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < beforeIds.size(); ++j) {
      if (afterIds[i] == beforeIds[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      selection.Select(afterIds[i]);
      return;
    }
  }
}
void EditorApp::DuplicateViaCommand(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    Announce(LogLevel::Warning, "Cannot duplicate: entity not found");
    return;
  }
  std::string name = e->name;
  std::unique_ptr<Command> cmd(new DuplicateCmd(id));
  std::vector<EntityId> beforeIds = scene.All();
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  SelectNewEntity(beforeIds);
  Announce(LogLevel::Success, std::string("Duplicated ") + name);
}
void EditorApp::DeleteViaCommand(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    Announce(LogLevel::Warning, "Cannot delete: entity not found");
    return;
  }
  std::string name = e->name;
  std::unique_ptr<Command> cmd(new DeleteEntityCmd(id));
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  selection.OnEntityDeleted(id);
  SyncSelection();
  Announce(LogLevel::Warning, std::string("Deleted ") + name);
}
void EditorApp::FocusEntity(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    return;
  }
  camera.Focus(e->transform.position[0], e->transform.position[1], e->transform.position[2], 5.0f);
  selection.Select(id);
  log.Add(LogLevel::Info, std::string("Focused ") + e->name);
}
void EditorApp::Frame() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::DockSpaceOverViewport(0, viewport);
  ProcessQueuedDialogs();
  if (savePromptQueued) {
    savePromptQueued = false;
    ImGui::OpenPopup("Unsaved Changes");
  }
  if (restorePromptQueued) {
    restorePromptQueued = false;
    ImGui::OpenPopup("Restore Recovery");
  }
  ImGuiIO& keysIo = ImGui::GetIO();
  if (!keysIo.WantTextInput && keysIo.KeyCtrl) {
    if (keysIo.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      DoRedo();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      DoUndo();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      DoRedo();
    }
  }
  DrawMenuBar();
  if (showViewport) {
    DrawViewport();
  }
  if (showHierarchy) {
    DrawHierarchy();
  }
  if (showInspector) {
    DrawInspector();
  }
  if (showConsole) {
    DrawConsole();
  }
  if (showAssetBrowser) {
    DrawAssetBrowser();
  }
  if (showSettings) {
    DrawSettings();
  }
  if (showAbout) {
    ImGui::Begin("About Kizuri", &showAbout);
    ImGui::Text("Kizuri Engine - Editor Shell Fase 3");
    ImGui::Text("ImGui + ImGuizmo + Deferred PBR");
    ImGui::End();
  }
  RefreshTitle();
  DrawSavePrompt();
  DrawRestorePrompt();
  DrawToasts();
}
void EditorApp::DrawMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene")) {
        RequestAction(1);
      }
      if (ImGui::MenuItem("Open...")) {
        RequestAction(2);
      }
      if (ImGui::MenuItem("Save")) {
        if (currentPath.empty()) {
          saveDialogPrefill.clear();
          saveDialogQueued = true;
          afterSaveRunPending = false;
        } else {
          DoSaveTo(currentPath);
        }
      }
      if (ImGui::MenuItem("Save As...")) {
        saveDialogPrefill = currentPath;
        saveDialogQueued = true;
        afterSaveRunPending = false;
      }
      if (ImGui::BeginMenu("Autosave")) {
        double iv = autosave.Interval();
        if (ImGui::MenuItem("Off", nullptr, iv <= 0.0)) {
          autosave.SetInterval(0.0);
        }
        if (ImGui::MenuItem("Every 1 min", nullptr, iv == 60.0)) {
          autosave.SetInterval(60.0);
        }
        if (ImGui::MenuItem("Every 5 min", nullptr, iv == 300.0)) {
          autosave.SetInterval(300.0);
        }
        if (ImGui::MenuItem("Every 10 min", nullptr, iv == 600.0)) {
          autosave.SetInterval(600.0);
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Exit")) {
        RequestAction(3);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      std::string undoLabel = std::string("Undo ") + undo.UndoName();
      std::string redoLabel = std::string("Redo ") + undo.RedoName();
      if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, undo.CanUndo())) {
        DoUndo();
      }
      if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, undo.CanRedo())) {
        DoRedo();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem("Viewport", nullptr, &showViewport);
      ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
      ImGui::MenuItem("Inspector", nullptr, &showInspector);
      ImGui::MenuItem("Console", nullptr, &showConsole);
      ImGui::MenuItem("Asset Browser", nullptr, &showAssetBrowser);
      ImGui::MenuItem("Settings", nullptr, &showSettings);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        showAbout = true;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}
void EditorApp::RefreshTitle() {
  bool dirty = scene.IsDirty();
  if (dirty == titleDirtyShown && currentPath == titlePathShown) {
    return;
  }
  titleDirtyShown = dirty;
  titlePathShown = currentPath;
  std::string name = currentPath.empty() ? "Untitled" : currentPath;
  size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  if (dirty) {
    name += "*";
  }
  std::string title = "Kizuri Editor - " + name;
  wchar_t wide[1024];
  int n = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide, 1023);
  if (n <= 0) {
    return;
  }
  wide[1023] = L'\0';
  SetWindowTextW(static_cast<HWND>(window.NativeHandle()), wide);
}
void EditorApp::RequestAction(int action) {
  if (scene.IsDirty()) {
    pendingAction = action;
    savePromptQueued = true;
    return;
  }
  pendingAction = action;
  RunPendingAction();
}
void EditorApp::RunPendingAction() {
  int action = pendingAction;
  pendingAction = 0;
  if (action == 1) {
    DoNewScene();
  } else if (action == 2) {
    openDialogQueued = true;
  } else if (action == 3) {
    running = false;
  }
}
void EditorApp::ProcessQueuedDialogs() {
  if (openDialogQueued) {
    openDialogQueued = false;
    std::string path;
    if (ShowOpenSceneDialog(window.NativeHandle(), path)) {
      DoOpenPath(path);
    } else if (GetLastDialogError() != 0) {
      log.Add(LogLevel::Error, std::string("Open dialog failed: ") + std::to_string(GetLastDialogError()));
    }
  }
  if (saveDialogQueued) {
    saveDialogQueued = false;
    std::string path = saveDialogPrefill;
    if (ShowSaveSceneDialog(window.NativeHandle(), path)) {
      DoSaveTo(path);
      if (afterSaveRunPending) {
        afterSaveRunPending = false;
        RunPendingAction();
      }
    } else {
      afterSaveRunPending = false;
      if (GetLastDialogError() != 0) {
        log.Add(LogLevel::Error, std::string("Save dialog failed: ") + std::to_string(GetLastDialogError()));
      }
    }
  }
}
void EditorApp::InitStoragePaths() {
  wchar_t tmp[1024];
  DWORD n = GetTempPathW(1024, tmp);
  std::string tmpDir = ".";
  if (n > 0 && n < 1024) {
    char narrow[1024];
    int m = WideCharToMultiByte(CP_UTF8, 0, tmp, -1, narrow, 1024, nullptr, nullptr);
    if (m > 1) {
      tmpDir = narrow;
    }
  }
  std::filesystem::path p(tmpDir);
  p /= "Kizuri";
  tmpAutosaveDir = p.string();
  std::error_code ec;
  std::filesystem::create_directories(tmpAutosaveDir, ec);
  wchar_t base[1024];
  DWORD b = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Kizuri", base, 1024);
  std::string appDir = tmpAutosaveDir;
  if (b > 0 && b < 1024) {
    char narrow[1024];
    int m = WideCharToMultiByte(CP_UTF8, 0, base, -1, narrow, 1024, nullptr, nullptr);
    if (m > 1) {
      appDir = narrow;
    }
  }
  std::filesystem::create_directories(appDir, ec);
  std::filesystem::path sp(appDir);
  sp /= "lastscene.txt";
  sessionFilePath = sp.string();
}
std::string EditorApp::ReadLastScene() {
  if (sessionFilePath.empty()) {
    return "";
  }
  FILE* fp = std::fopen(sessionFilePath.c_str(), "rb");
  if (fp == nullptr) {
    return "";
  }
  char buf[1024];
  size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
  std::fclose(fp);
  buf[n] = '\0';
  std::string out(buf);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
    out.pop_back();
  }
  return out;
}
void EditorApp::WriteLastScene(const std::string& path) {
  if (sessionFilePath.empty()) {
    return;
  }
  FILE* fp = std::fopen(sessionFilePath.c_str(), "wb");
  if (fp == nullptr) {
    return;
  }
  std::fwrite(path.c_str(), 1, path.size(), fp);
  std::fclose(fp);
}
std::string EditorApp::CurrentRecoveryPath() {
  return RecoveryPathFor(currentPath, tmpAutosaveDir);
}
void EditorApp::OfferRestoreFor(const std::string& mainPath) {
  std::string rec = RecoveryPathFor(mainPath, tmpAutosaveDir);
  if (ShouldOfferRecovery(mainPath, rec)) {
    pendingRestoreMain = mainPath;
    restorePromptQueued = true;
  }
}
void EditorApp::DrawRestorePrompt() {
  if (ImGui::BeginPopupModal("Restore Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    std::string name = pendingRestoreMain.empty() ? "Untitled" : pendingRestoreMain;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    ImGui::Text("Found a newer autosave for %s.", name.c_str());
    ImGui::Text("Restore it?");
    if (ImGui::Button("Restore")) {
      std::string rec = RecoveryPathFor(pendingRestoreMain, tmpAutosaveDir);
      if (LoadSceneFromFile(scene, rec, &log)) {
        currentPath = pendingRestoreMain;
        scene.MarkDirty();
        selection.Clear();
        edits.Clear();
        undo.Clear();
        autosave.ResetTimer();
        WriteLastScene(currentPath);
        Announce(LogLevel::Success, "Recovery restored");
      } else {
        Announce(LogLevel::Error, "Recovery restore failed");
      }
      DeleteRecoveryFile(rec);
      pendingRestoreMain.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      DeleteRecoveryFile(RecoveryPathFor(pendingRestoreMain, tmpAutosaveDir));
      pendingRestoreMain.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
void EditorApp::DoNewScene() {
  scene.Clear();
  selection.Clear();
  edits.Clear();
  undo.Clear();
  currentPath.clear();
  autosave.ResetTimer();
  WriteLastScene("");
  log.Add(LogLevel::Warning, "Scene cleared");
}
void EditorApp::DoSaveTo(const std::string& path) {
  if (SaveSceneToFile(scene, path)) {
    currentPath = path;
    scene.ClearDirty();
    autosave.ResetTimer();
    DeleteRecoveryFile(RecoveryPathFor(path, tmpAutosaveDir));
    WriteLastScene(path);
    Announce(LogLevel::Success, std::string("Scene saved: ") + path);
  } else {
    Announce(LogLevel::Error, std::string("Scene save failed: ") + path);
  }
}
void EditorApp::DoOpenPath(const std::string& path) {
  if (LoadSceneFromFile(scene, path, &log)) {
    currentPath = path;
    selection.Clear();
    edits.Clear();
    undo.Clear();
    autosave.ResetTimer();
    WriteLastScene(path);
    Announce(LogLevel::Success, std::string("Scene opened: ") + path);
    OfferRestoreFor(path);
  } else {
    Announce(LogLevel::Error, std::string("Scene open failed: ") + path);
  }
}
void EditorApp::Announce(LogLevel level, const std::string& text) {
  log.Add(level, text);
  notifications.Notify(level, text);
}
void EditorApp::DrawToasts() {
  std::vector<Notification> active = notifications.Active();
  if (active.empty()) {
    return;
  }
  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x - 16.0f, vp->WorkPos.y + vp->WorkSize.y - 16.0f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("##toasts", nullptr, flags)) {
    for (size_t i = 0; i < active.size(); ++i) {
      ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
      const char* tag = "INFO";
      if (active[i].level == LogLevel::Success) {
        color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        tag = "OK";
      } else if (active[i].level == LogLevel::Warning) {
        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        tag = "WARN";
      } else if (active[i].level == LogLevel::Error) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        tag = "ERROR";
      }
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s] %s", tag, active[i].text.c_str());
      ImGui::PopStyleColor();
    }
  }
  ImGui::End();
}
void EditorApp::DrawSavePrompt() {
  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Save changes before continuing?");
    if (ImGui::Button("Save")) {
      if (currentPath.empty()) {
        saveDialogPrefill.clear();
        saveDialogQueued = true;
        afterSaveRunPending = true;
        ImGui::CloseCurrentPopup();
      } else {
        DoSaveTo(currentPath);
        ImGui::CloseCurrentPopup();
        RunPendingAction();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save")) {
      ImGui::CloseCurrentPopup();
      RunPendingAction();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      pendingAction = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
}
