#pragma once
#include "Kizuri/Audio.h"
#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Camera.h"
#include "Kizuri/DeferredRenderer.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/PhysicsWorld.h"
#include "Kizuri/QualityPreset.h"
#include "Kizuri/Scene.h"
#include <string>
#include <unordered_map>
namespace Kizuri {
bool SubmitSceneLights(DeferredRenderer& renderer, const Scene& scene, const EntityId& soloLight);
struct GameWorldOptions {
  bool loadAssets = true;
  bool enablePhysics = true;
  bool enableAudio = true;
  bool lowSpec = false;
  float renderScale = 1.0f;
  int shadowAtlasSize = 4096;
};
void ApplyQualityToRenderer(DeferredRenderer& renderer, const QualitySettings& q);
class GameWorld {
public:
  GameWorld();
  ~GameWorld();
  bool Initialize(IRHI* rhi, int width, int height, const std::string& shaderDir, const std::string& assetsDir, const GameWorldOptions& options);
  void Shutdown();
  bool LoadSceneFile(const std::string& path);
  void CreateFallbackScene();
  void SetViewSize(int width, int height);
  void SetCameraPosition(float x, float y, float z);
  void SetCameraYawPitch(float yaw, float pitch);
  void Update(float dt);
  void Render(float alpha);
  void SyncPhysics(float dt);
  void SyncAudio(float dt);
  Scene& GetScene() { return scene; }
  const Scene& GetScene() const { return scene; }
  DeferredRenderer& GetRenderer() { return renderer; }
  PhysicsWorld& GetPhysics() { return physics; }
  AudioEngine& GetAudio() { return audio; }
  FreeCamera& GetCamera() { return camera; }
  AssetDatabase& GetAssets() { return assets; }
  JobSystem& GetJobs() { return jobs; }
  const std::string& LastError() const { return lastError; }
  bool IsInitialized() const { return initialized; }
private:
  Scene scene;
  DeferredRenderer renderer;
  AssetDatabase assets;
  JobSystem jobs;
  PhysicsWorld physics;
  AudioEngine audio;
  FreeCamera camera;
  std::string lastError;
  std::unordered_map<EntityId, uint32_t, EntityIdHash> physBodies;
  bool initialized = false;
  int viewWidth = 1280;
  int viewHeight = 720;
};
}