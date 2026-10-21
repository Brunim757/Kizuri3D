#include "Kizuri/GameWorld.h"
#include "Kizuri/GameMaterial.h"
#include "Kizuri/SceneSerializer.h"
#include <DirectXMath.h>
#include <cmath>
#include <unordered_map>
#include <vector>
namespace Kizuri {
bool SubmitSceneLights(DeferredRenderer& renderer, const Scene& scene, const EntityId& soloLight) {
  renderer.ClearLights();
  bool any = false;
  const std::vector<EntityId> ids = scene.All();
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    if (soloLight.IsValid()) {
      if (ids[i] != soloLight) {
        continue;
      }
    }
    any = true;
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
      EntityForward(e->transform, sl.dir);
      renderer.AddSpotLight(sl);
    } else if (e->light.type == static_cast<int>(LightType::Directional)) {
      RenderDirectionalLight dl;
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
      EntityForward(e->transform, dl.dir);
      renderer.AddDirectionalLight(dl);
    } else if (e->light.type == static_cast<int>(LightType::Rect)) {
      RenderRectLight rl;
      rl.pos[0] = e->transform.position[0];
      rl.pos[1] = e->transform.position[1];
      rl.pos[2] = e->transform.position[2];
      rl.color[0] = e->light.color[0];
      rl.color[1] = e->light.color[1];
      rl.color[2] = e->light.color[2];
      rl.intensity = e->light.intensity;
      rl.halfW = e->light.areaW * 0.5f;
      rl.halfH = e->light.areaH * 0.5f;
      rl.range = e->light.radius;
      rl.linkMask = static_cast<uint32_t>(e->light.linkMask);
      rl.layer = static_cast<uint32_t>(e->light.layer);
      EntityForward(e->transform, rl.normal);
      renderer.AddRectLight(rl);
    }
  }
  return any;
}
GameWorld::GameWorld() {
}
GameWorld::~GameWorld() {
  Shutdown();
}
void ApplyQualityToRenderer(DeferredRenderer& renderer, const QualitySettings& q) {
  renderer.SetSsao(q.ssao, 1.0f, 1.0f);
  renderer.SetAoMode(q.aoMode);
  renderer.SetSsr(q.ssr);
  renderer.SetSsrTemporal(q.ssrTemporal, 0.9f);
  renderer.SetContact(q.contact);
  renderer.SetSsgi(q.ssgi);
  renderer.SetDof(q.dof, 10.0f, 10.0f);
  renderer.SetMotionBlur(q.motionBlur, 0.5f);
  renderer.SetFxaa(q.fxaa);
  renderer.SetTaa(q.taa);
  renderer.SetTaaSharpening(q.taaSharp);
  renderer.SetBloom(q.bloom);
  renderer.SetShadowFilter(q.shadowFilter);
  renderer.SetDynamicResolution(q.dynRes, 16.6f, 0.5f, 1.0f);
  renderer.SetOcclusionCulling(q.occ);
  renderer.SetGodRays(q.godRays, 0.5f, 0.9f, 0.2f);
  renderer.SetFsr(q.renderScale, 0.5f);
}
bool GameWorld::Initialize(IRHI* rhi, int width, int height, const std::string& shaderDir, const std::string& assetsDir, const GameWorldOptions& options) {
  if (initialized) {
    return true;
  }
  if (rhi == nullptr) {
    lastError = "RHI nulo";
    return false;
  }
  viewWidth = width > 0 ? width : 1280;
  viewHeight = height > 0 ? height : 720;
  if (!renderer.Initialize(rhi, viewWidth, viewHeight, shaderDir.c_str())) {
    lastError = "Falha ao inicializar o renderer em " + shaderDir;
    return false;
  }
  if (options.lowSpec) {
    QualitySettings low = ApplyQualityPreset(QualityLevel::Low);
    low.renderScale = options.renderScale > 0.0f ? options.renderScale : 0.5f;
    ApplyQualityToRenderer(renderer, low);
  }
  if (options.loadAssets && !assetsDir.empty()) {
    assets.SetAssetsDir(assetsDir);
    assets.SetGpuRHI(rhi);
    assets.Scan();
    assets.DrainBlocking();
  }
  if (!jobs.Initialize()) {
    lastError = "Falha ao iniciar o Job System";
    return false;
  }
  if (options.enablePhysics && !physics.Initialize(&jobs)) {
    lastError = "Falha ao inicializar a fisica";
    return false;
  }
  if (options.enableAudio) {
    audio.Initialize(48000, 2);
  }
  camera.SetPosition(0.0f, 4.0f, -10.0f);
  camera.SetYawPitch(0.0f, 0.15f);
  initialized = true;
  return true;
}
void GameWorld::Shutdown() {
  if (!initialized) {
    return;
  }
  initialized = false;
  audio.Shutdown();
  physics.Shutdown();
  jobs.Shutdown();
  assets.SetGpuRHI(nullptr);
  renderer.Shutdown();
}
bool GameWorld::LoadSceneFile(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  if (!LoadSceneFromFile(scene, path, nullptr)) {
    lastError = "Falha ao carregar a cena " + path;
    return false;
  }
  return true;
}
void GameWorld::CreateFallbackScene() {
  EntityId id = scene.CreateEntity("Directional");
  Entity* e = scene.Get(id);
  if (e == nullptr) {
    return;
  }
  e->hasLight = true;
  MakeDefaultLight(e->light);
  e->light.type = static_cast<int>(LightType::Directional);
  e->light.intensity = 3.0f;
  e->light.color[0] = 1.0f;
  e->light.color[1] = 0.96f;
  e->light.color[2] = 0.9f;
  e->transform.rotation[1] = 45.0f;
  e->transform.rotation[0] = -35.0f;
}
void GameWorld::SetViewSize(int width, int height) {
  if (width > 0 && height > 0) {
    viewWidth = width;
    viewHeight = height;
  }
}
void GameWorld::SetCameraPosition(float x, float y, float z) {
  camera.SetPosition(x, y, z);
}
void GameWorld::SetCameraYawPitch(float yaw, float pitch) {
  camera.SetYawPitch(yaw, pitch);
}
void GameWorld::Update(float dt) {
  if (!initialized) {
    return;
  }
  SyncPhysics(dt);
  SyncAudio(dt);
}
void GameWorld::SyncPhysics(float dt) {
  if (!initialized || !physics.IsInitialized()) {
    return;
  }
  const std::vector<EntityId> ids = scene.All();
  for (size_t i = 0; i < ids.size(); ++i) {
    Entity* e = scene.Get(ids[i]);
    if (e == nullptr || !e->hasRigidbody) {
      continue;
    }
    float quat[4];
    EulerDegToQuat(e->transform.rotation, quat);
    uint32_t body = 0;
    std::unordered_map<EntityId, uint32_t, EntityIdHash>::const_iterator it = physBodies.find(e->id);
    if (it != physBodies.end() && physics.HasBody(it->second)) {
      body = it->second;
    }
    if (body == 0) {
      PhysBodyDesc desc;
      desc.motion = e->rigidbody.motionType == 0 ? PhysMotionType::Static : (e->rigidbody.motionType == 1 ? PhysMotionType::Kinematic : PhysMotionType::Dynamic);
      desc.mass = e->rigidbody.mass;
      desc.collider = e->rigidbody.colliderType == 1 ? PhysColliderType::Sphere : (e->rigidbody.colliderType == 2 ? PhysColliderType::Capsule : (e->rigidbody.colliderType == 3 ? PhysColliderType::Mesh : PhysColliderType::Box));
      desc.extents[0] = e->rigidbody.extents[0];
      desc.extents[1] = e->rigidbody.extents[1];
      desc.extents[2] = e->rigidbody.extents[2];
      desc.friction = e->rigidbody.friction;
      desc.restitution = e->rigidbody.restitution;
      const uint32_t created = physics.CreateBody(desc, e->transform.position, quat);
      if (created == 0xFFFFFFFFu) {
        continue;
      }
      physBodies[e->id] = created;
      body = created;
    }
    if (e->rigidbody.motionType != 2) {
      physics.TeleportBody(body, e->transform.position, quat);
    }
    if (e->rigidbody.motionType == 1) {
      physics.SetKinematicTarget(body, e->transform.position, quat);
    }
  }
  for (std::unordered_map<EntityId, uint32_t, EntityIdHash>::iterator it = physBodies.begin(); it != physBodies.end();) {
    const Entity* e = scene.Get(it->first);
    if (e == nullptr || !e->hasRigidbody) {
      physics.DestroyBody(it->second);
      it = physBodies.erase(it);
    } else {
      ++it;
    }
  }
  physics.Step(dt);
  for (std::unordered_map<EntityId, uint32_t, EntityIdHash>::const_iterator it = physBodies.begin(); it != physBodies.end(); ++it) {
    Entity* e = scene.Get(it->first);
    if (e == nullptr || e->rigidbody.motionType != 2) {
      continue;
    }
    float pos[3] = { 0.0f, 0.0f, 0.0f };
    float quat[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (!physics.GetPose(it->second, pos, quat)) {
      continue;
    }
    e->transform.position[0] = pos[0];
    e->transform.position[1] = pos[1];
    e->transform.position[2] = pos[2];
  }
}
void GameWorld::SyncAudio(float dt) {
  (void)dt;
  if (!initialized || !audio.IsInitialized()) {
    return;
  }
  audio.Update();
}
void GameWorld::Render(float alpha) {
  (void)alpha;
  if (!initialized) {
    return;
  }
  SubmitSceneLights(renderer, scene, EntityId::Invalid());
  const std::vector<EntityId> ids = scene.All();
  float world[16];
  DeferredMaterial material;
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr || !e->hasMesh || e->meshGuid.empty()) {
      continue;
    }
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec == nullptr || !rec->loaded) {
      continue;
    }
    if (rec->gpuVB == 0) {
      assets.EnsureMeshGpu(e->meshGuid, 64u * 1024u * 1024u);
      rec = assets.GetByGuid(e->meshGuid);
      if (rec == nullptr || rec->gpuVB == 0) {
        continue;
      }
    }
    ComposeMatrix(e->transform, world);
    FillDeferredFromEntity(assets, e->material, material);
    renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, 0, rec->gpuCount, &material, 0.0f, rec->data.aabbMin, rec->data.aabbMax);
  }
  float view[16];
  float proj[16];
  const float aspect = static_cast<float>(viewWidth) / (viewHeight > 0 ? static_cast<float>(viewHeight) : 1.0f);
  DirectX::XMFLOAT4X4 viewMat;
  DirectX::XMFLOAT4X4 projMat;
  DirectX::XMStoreFloat4x4(&viewMat, camera.View());
  DirectX::XMStoreFloat4x4(&projMat, camera.Projection(aspect));
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      view[r * 4 + c] = viewMat.m[r][c];
      proj[r * 4 + c] = projMat.m[r][c];
    }
  }
  float camPos[3];
  camera.GetPosition(camPos[0], camPos[1], camPos[2]);
  renderer.Render(view, proj, camPos);
}
}