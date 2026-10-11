#pragma once
#include "Kizuri/Assets/MeshCodec.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Allocator.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/RHI.h"
#include <TaskScheduler.h>
#include <atomic>
#include <cstdint>
#include <map>
#include <utility>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
namespace Kizuri {
enum class MeshAssetState {
  Ready,
  Outdated,
  NoSource,
  SourceMissing
};
struct MeshRecord {
  std::string guid;
  std::string meshPath;
  std::string sourcePath;
  uint64_t sourceHash;
  bool hasSource;
  MeshAssetState state;
  MeshAssetData data;
  bool loaded;
  int64_t lastSeenSourceTime;
  RHIBuffer gpuVB;
  RHIBuffer gpuIB;
  uint32_t gpuCount;
  bool gpuReady;
  bool meshletsBuilt = false;
  std::vector<MeshletRange> meshlets;
  std::vector<float> stageInterleaved;
  size_t stageVBDone;
  size_t stageIBDone;
};
struct RefUse {
  std::string userLabel;
  std::string assetGuid;
};
struct ImportResult {
  bool ok;
  size_t taskId;
  std::string sourcePath;
  std::string meshPath;
  MeshAssetData data;
  std::string warning;
  std::string error;
};
struct ImportMeshTask : public enki::ITaskSet {
  ImportMeshTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  std::string sourcePath;
  std::string meshPath;
  std::string keepGuid;
  size_t taskId;
  std::atomic<int> progress;
  std::mutex* outMutex;
  std::vector<ImportResult>* outQueue;
};
struct LodBakeTask : public enki::ITaskSet {
  LodBakeTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  MeshAssetData baseData;
  std::string meshPath;
  size_t taskId;
  std::atomic<int> progress;
  std::mutex* outMutex;
  std::vector<ImportResult>* outQueue;
};
enum class TextureAssetState {
  Ready,
  Outdated,
  NoSource,
  SourceMissing
};
struct TextureRecord {
  std::string guid;
  std::string texPath;
  std::string sourcePath;
  uint64_t sourceHash;
  bool hasSource;
  TextureAssetState state;
  TextureAssetData data;
  bool loaded;
  int64_t lastSeenSourceTime;
  RHITexture gpu;
  int residentLevels;
  int selectedLevels;
};
struct ImportTexResult {
  bool ok;
  size_t taskId;
  std::string sourcePath;
  std::string texPath;
  TextureAssetData data;
  std::string error;
};
struct ImportTexTask : public enki::ITaskSet {
  ImportTexTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  std::string sourcePath;
  std::string texPath;
  std::string keepGuid;
  size_t taskId;
  std::atomic<int> progress;
  std::mutex* outMutex;
  std::vector<ImportTexResult>* outQueue;
};
struct MeshUse {
  std::string meshGuid;
  float pos[3];
  bool tierOnly = false;
};
struct VtBundle {
  RHITexture physical = 0;
  RHITexture table = 0;
  RHITexture tail = 0;
  float residentMip = 99.0f;
  bool valid = false;
};
struct TexUpload {
  std::string guid;
  int mip;
};
struct HashResult {
  bool ok;
  size_t taskId;
  std::string path;
  std::string guid;
  bool isTex;
  uint64_t hash;
  int64_t fileTime;
};
struct HashTask : public enki::ITaskSet {
  HashTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  std::string path;
  std::string guid;
  bool isTex;
  size_t taskId;
  std::atomic<int> progress;
  std::mutex* outMutex;
  std::vector<HashResult>* outQueue;
};
class AssetDatabase;
constexpr size_t TaskBlockMax(size_t a, size_t b) {
  return a > b ? a : b;
}
struct TaskMemory {
  static void* Allocate(AssetDatabase* db, size_t size, size_t alignment);
  static void Release(AssetDatabase* db, void* p);
};
template<typename T>
struct TaskDeleter {
  AssetDatabase* db = nullptr;
  void operator()(T* p) const {
    if (p == nullptr) {
      return;
    }
    p->~T();
    TaskMemory::Release(db, p);
  }
};
template<typename T>
using TaskPtr = std::unique_ptr<T, TaskDeleter<T>>;
class AssetDatabase {
public:
  AssetDatabase();
  void SetAssetsDir(const std::string& dir);
  const std::string& AssetsDir() const;
  void Scan();
  size_t DrainCompleted();
  size_t PendingImports() const;
  void DrainBlocking();
  std::vector<std::pair<std::string, int>> ImportingNow() const;
  const MeshRecord* GetByGuid(const std::string& guid) const;
  const MeshRecord* GetByMeshPath(const std::string& path) const;
  std::vector<std::string> AllGuids() const;
  bool Reimport(const std::string& guid);
  size_t RelocateMissing();
  std::vector<std::string> TakeRelocated();
  std::vector<std::string> TakeImportErrors();
  std::vector<std::string> TakeImportWarnings();
  bool RenameAssetFile(const std::string& guid, const std::string& newFileName);
  bool SetSourcePath(const std::string& guid, const std::string& newSourcePath);
  bool SetTexSourcePath(const std::string& guid, const std::string& newSourcePath);
  bool DeleteAssetFile(const std::string& guid, bool force, const std::vector<RefUse>& refs, std::vector<std::string>& blockedBy);
  std::map<std::string, size_t> ComputeRefCounts(const std::vector<RefUse>& refs) const;
  const TextureRecord* GetTexByGuid(const std::string& guid) const;
  std::vector<std::string> AllTexGuids() const;  bool ReimportTexture(const std::string& guid);
  bool RenameTextureFile(const std::string& guid, const std::string& newFileName);
  bool DeleteTextureFile(const std::string& guid, bool force, std::vector<std::string>& blockedBy);
  std::map<std::string, size_t> ComputeTexRefCounts() const;
  static int SelectMipLevel(float dist, int mipCount);
  void RequestTexture(const std::string& guid, float dist);
  bool EnsureTextureKind(const std::string& guid, TexKind kind);
  void SetGpuRHI(IRHI* rhi);
  RHITexture ImportHdrSky(const std::string& path);
  RHITexture ImportHdrSkyFromMemory(const unsigned char* data, size_t size);
  RHITexture EquirectToCube(const float* hdr, int w, int h);
  bool EnsureMeshGpu(const std::string& guid, size_t maxBytes);
  void DropMeshGpu(MeshRecord& record);
  void UpsertBatchMesh(const std::string& key, const MeshAssetData& data);
  void DiscardBatchMeshes();
  RHITexture BuildCookieArray(const std::vector<const TextureAssetData*>& slices);
  std::string CreateProceduralTexture(int mode, int w, int h);
  RHITexture BuildDecalAtlas(const std::vector<const TextureAssetData*>& textures, std::vector<float>& outRects);
  int CreateVirtualTexture(const TextureAssetData* src, const std::string& texGuid);
  bool UpdateVirtualTexture(const std::string& texGuid, float dist, int budget = 8);
  bool GetVirtualBundle(const std::string& texGuid, VtBundle& out);
  void SetMeshStreaming(bool on, float distance);
  void SetGeometryStreaming(bool on, float distance);
  bool MeshStreamingOn() const { return meshStreaming; }
  bool GeometryStreamingOn() const { return geoStreaming; }
  uint64_t StreamEvictions() const { return streamEvictions; }
  void DropTexGpu(TextureRecord& record);
  void UpdateStreaming(const float cameraPos[3], const std::vector<MeshUse>& uses);
  size_t DrainUploads(IRHI* rhi, size_t maxBytes);
private:
  IRHI* gpuRhi;
  std::string assetsDir;
  std::map<std::string, MeshRecord> records;
  JobSystem jobs;
  bool jobsReady;
  PoolAllocator taskPool;
  ArenaAllocator skyScratch;
  size_t nextTaskId;
  std::vector<TaskPtr<ImportMeshTask>> pending;
  std::vector<TaskPtr<LodBakeTask>> pendingLod;
  std::vector<std::string> inflight;
  std::vector<TaskPtr<ImportTexTask>> pendingTex;
  std::vector<std::string> inflightTex;
  std::vector<TaskPtr<HashTask>> pendingHash;
  std::mutex completedTexMutex;
  std::vector<ImportTexResult> completedTex;
  std::mutex completedHashMutex;
  std::vector<HashResult> completedHash;
  std::map<std::string, int64_t> failedDecode;
  std::map<std::string, int64_t> failedImport;
  std::map<std::string, TextureRecord> texRecords;
  std::map<std::string, int64_t> seenTexTime;
  std::map<std::string, std::string> texPathToGuid;
  std::vector<TexUpload> uploadQueue;
  std::mutex completedMutex;
  std::vector<ImportResult> completed;
  std::vector<std::string> relocated;
  std::vector<std::string> importErrors;
  std::vector<std::string> importWarnings;
  std::map<std::string, int64_t> seenMeshTime;
  std::map<std::string, std::string> pathToGuid;
  bool meshStreaming = false;
  float meshStreamDist = 150.0f;
  bool geoStreaming = false;
  float geoStreamDist = 80.0f;
  uint64_t streamEvictions = 0;
  RHITexture cookieArrayTex = 0;
  RHITexture decalAtlasTex = 0;
  struct VtEntry {
    std::string texGuid;
    int w = 0;
    int h = 0;
    int mipCount = 0;
    int residentMip = 99;
    RHITexture physical = 0;
    RHITexture tableTex = 0;
    RHITexture tailTex = 0;
    std::vector<unsigned char> tableCpu;
    std::vector<unsigned char> mipRgba;
    int mipRgbaW = 0;
    int mipRgbaH = 0;
    int mipRgbaMip = -1;
    int nextPage = 0;
    int pagesX = 0;
    int pagesY = 0;
  };
  std::vector<VtEntry> vtEntries;
  std::map<std::string, size_t> vtIndex;
  void EnsureJobs();
  void* AllocTaskMemory(size_t size, size_t alignment);
  void FreeTaskMemory(void* p);
  friend struct TaskMemory;
  template<typename T>
  TaskPtr<T> MakeTask() {
    void* mem = AllocTaskMemory(sizeof(T), alignof(T));
    if (mem == nullptr) {
      return TaskPtr<T>(nullptr, TaskDeleter<T>{ this });
    }
    T* obj = new (mem) T();
    return TaskPtr<T>(obj, TaskDeleter<T>{ this });
  }
  void EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid);
  void EnqueueLodBake(const MeshAssetData& baseData, const std::string& meshPath);  void EnqueueTexImport(const std::string& sourcePath, const std::string& texPath, const std::string& keepGuid);
  void EnqueueDecode(const std::string& compiledPath, bool isTex);
  void EnqueueHash(const std::string& path, const std::string& guid, bool isTex);
  bool HashPendingForPath(const std::string& path) const;
  bool ImportPendingForPath(const std::string& path) const;
  bool DecodePendingFor(const std::string& path) const;
  bool SiblingUsable(const std::string& sourcePath, bool isTex) const;
  bool TryReconnectByHash(const std::string& path, uint64_t hash, int64_t fileTime);
  void UpsertResult(const ImportResult& result);
  void UpsertTexResult(const ImportTexResult& result);
  MeshRecord* FindByGuid(const std::string& guid);
  void InitFreshMeshState(MeshRecord& record);
  void InitFreshTexState(TextureRecord& record);
};
}
