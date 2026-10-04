#pragma once
#include "Kizuri/Scene.h"
#include "Kizuri/MultiSelection.h"
#include "Kizuri/Log.h"
#include "Kizuri/EditQueue.h"
#include "Kizuri/Undo.h"
#include "Kizuri/Autosave.h"
#include "Kizuri/Notifications.h"
#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Window.h"
#include "Kizuri/RHI.h"
#include "Kizuri/Camera.h"
#include "Kizuri/MeshLoader.h"
#include "Kizuri/DeferredRenderer.h"
#include <string>
#include <unordered_map>
namespace Kizuri {
struct EditorApp {
  Window window;
  IRHI* rhi;
  DeferredRenderer renderer;
  Scene scene;
  SelectionSet selection;
  LogStore log;
  NotificationCenter notifications;
  AssetDatabase assets;
  bool showNotifHistory;
  EditQueue edits;
  UndoStack undo;
  bool gizmoDragging;
  Transform gizmoStart;
  EntityId gizmoTarget;
  bool gizmoJustEnded;
  std::unordered_map<EntityId, Transform, EntityIdHash> gizmoOrigins;
  std::unordered_map<EntityId, float, EntityIdHash> decalAge;
  bool rubberActive;
  float rubberX0;
  float rubberY0;
  FreeCamera camera;
  StaticMesh cubeMesh;
  bool cubeReady;
  bool running;
  bool showHierarchy;
  bool showInspector;
  bool showConsole;
  bool showViewport;
  bool showAssetBrowser;
  bool showAbout;
  bool showSettings;
  bool debugCascades;
  float viewExposure;
  bool viewACES;
  float viewBloom;
  float viewScale;
  float viewSharp;
  bool viewSsao;
  float viewSsaoIntensity;
  float viewSsaoRadius;
  bool viewFog;
  float viewFogDensity;
  float viewFogColor[3];
  float viewX;
  float viewY;
  float viewW;
  float viewH;
  bool viewValid;
  float frameDt;
  int lastMouseX;
  int lastMouseY;
  bool downPosValid;
  int downX;
  int downY;
  int rdownX;
  int rdownY;
  bool rdownValid;
  EntityId contextPick;
  bool gizmoHotLast;
  int gizmoOp;
  char renameBuf[128];
  bool renameActive;
  EntityId renameTarget;
  char assetRenameBuf[256];
  std::string assetRenameGuid;
  bool assetRenameIsTex;
  bool assetRenameActive;
  std::string selectedAssetGuid;
  bool selectedAssetIsTex;
  std::string forceDeleteGuid;
  bool forceDeleteIsTex;
  RHIBuffer defaultVB;
  RHIBuffer defaultIB;
  uint32_t defaultCount;
  double scanTimer;
  std::string currentPath;
  bool titleDirtyShown;
  std::string titlePathShown;
  int pendingAction;
  bool openDialogQueued;
  bool saveDialogQueued;
  bool afterSaveRunPending;
  bool savePromptQueued;
  std::string saveDialogPrefill;
  AutosaveManager autosave;
  std::string tmpAutosaveDir;
  std::string sessionFilePath;
  std::string pendingRestoreMain;
  bool restorePromptQueued;
  EditorApp();
  bool Initialize();
  int Run();
  void Shutdown();
  void Frame();
  void DrawMenuBar();
  void DrawViewport();
  void DrawHierarchy();
  void DrawInspector();
  void DrawConsole();
  void DrawToasts();
  void Announce(LogLevel level, const std::string& text);
  void DrawAssetBrowser();
  void DrawSettings();
  void DrawMeshSection(Entity* e);
  void DrawMaterialSection(Entity* e);
  void DrawMaterialSlot(Entity* e, const char* label, int slot);
  void DrawDecalSection(Entity* e);
  void DrawLightGizmo();
  std::string ResolveAssetsDir();
  std::vector<RefUse> SceneMeshRefs();
  void PumpAssets();
  void UploadDefaultCube();
  void HandleOsDrop(void* hwnd);
  void RenderScene();
  void UpdateCamera(float dt, bool lookNow);
  void HandleViewportClick();
  void HandleRubberSelect(float x0, float y0, float x1, float y1);
  void CreateEntityAt(float x, float y, float z, const std::string& meshGuid = "");
  void FocusEntity(EntityId id);
  void RefreshTitle();
  void RequestAction(int action);
  void RunPendingAction();
  void DoSaveTo(const std::string& path);
  void DoOpenPath(const std::string& path);
  void DoNewScene();
  void DrawSavePrompt();
  void ProcessQueuedDialogs();
  void InitStoragePaths();
  std::string ReadLastScene();
  void WriteLastScene(const std::string& path);
  void OfferRestoreFor(const std::string& mainPath);
  void DrawRestorePrompt();
  std::string CurrentRecoveryPath();
  void DoUndo();
  void DoRedo();
  void SyncSelection();
  void SelectNewEntity(const std::vector<EntityId>& beforeIds);
  void DuplicateViaCommand(EntityId id);
  void DeleteViaCommand(EntityId id);
  std::string FindAsset(const char* name);
  std::string FindShaderDir();
};
}
