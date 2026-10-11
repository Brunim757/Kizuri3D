#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Path.h"
#include "Kizuri/Log.h"
#include <DirectXTex.h>
#include <stb_image.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <system_error>
namespace Kizuri {
namespace {
bool IsImageExt(const std::string& ext) {
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}
RHITextureFormat ToRHIFormat(TexFormat fmt, bool srgb) {
  if (fmt == TexFormat::Bc1) {
    return srgb ? RHITextureFormat::BC1_UNORM_SRGB : RHITextureFormat::BC1_UNORM;
  }
  if (fmt == TexFormat::Bc3) {
    return srgb ? RHITextureFormat::BC3_UNORM_SRGB : RHITextureFormat::BC3_UNORM;
  }
  if (fmt == TexFormat::Bc5) {
    return RHITextureFormat::BC5_UNORM;
  }
  if (fmt == TexFormat::Bc4) {
    return RHITextureFormat::BC4_UNORM;
  }
  if (fmt == TexFormat::Bc6H) {
    return RHITextureFormat::BC6H_UF16;
  }
  if (fmt == TexFormat::Bc7) {
    return srgb ? RHITextureFormat::BC7_UNORM_SRGB : RHITextureFormat::BC7_UNORM;
  }
  return srgb ? RHITextureFormat::RGBA8_UNORM_SRGB : RHITextureFormat::RGBA8_UNORM;
}
bool StatTime(const std::string& path, int64_t& ticks) {
  std::error_code ec;
  std::filesystem::file_time_type ft = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return false;
  }
  ticks = static_cast<int64_t>(ft.time_since_epoch().count());
  return true;
}
}
ImportMeshTask::ImportMeshTask()
  : taskId(0)
  , progress(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void ImportMeshTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  progress = 10;
  ImportResult result;
  result.ok = false;
  result.taskId = taskId;
  result.sourcePath = sourcePath;
  result.meshPath = meshPath;
  if (sourcePath.empty()) {
    try {
      if (DecodeMeshFile(meshPath, result.data) && !result.data.guid.empty()) {
        result.ok = true;
        result.sourcePath = meshPath;
      }
    } catch (const std::bad_alloc&) {
      result.error = "out of memory";
    } catch (...) {
      result.error = "decode failed";
    }
  } else {
    try {
      LogStore log;
      if (ImportGltfMesh(sourcePath, keepGuid, result.data, &log, &progress)) {
        if (EncodeMeshFile(result.data, meshPath)) {
          result.ok = true;
        }
      }
      std::string warn;
      size_t shown = 0;
      for (size_t i = 0; i < log.Count() && shown < 5; ++i) {
        if (log.At(i).level == LogLevel::Warning || log.At(i).level == LogLevel::Error) {
          bool dup = warn.find(log.At(i).text) != std::string::npos;
          if (!dup) {
            if (!warn.empty()) {
              warn += "\n";
            }
            warn += log.At(i).text;
            ++shown;
          }
        }
      }
      result.warning = warn;
    } catch (const std::bad_alloc&) {
      result.error = "out of memory";
    } catch (...) {
      result.error = "import failed";
    }
  }
  progress = 100;
  if (outMutex != nullptr && outQueue != nullptr) {
    std::lock_guard<std::mutex> lock(*outMutex);
    outQueue->push_back(result);
  }
}
namespace {
size_t TaskBlockSize() {
  size_t s = sizeof(ImportMeshTask);
  if (sizeof(LodBakeTask) > s) {
    s = sizeof(LodBakeTask);
  }
  if (sizeof(ImportTexTask) > s) {
    s = sizeof(ImportTexTask);
  }
  if (sizeof(HashTask) > s) {
    s = sizeof(HashTask);
  }
  return s;
}
size_t TaskBlockAlign() {
  size_t a = alignof(ImportMeshTask);
  if (alignof(LodBakeTask) > a) {
    a = alignof(LodBakeTask);
  }
  if (alignof(ImportTexTask) > a) {
    a = alignof(ImportTexTask);
  }
  if (alignof(HashTask) > a) {
    a = alignof(HashTask);
  }
  return a;
}
}
void* TaskMemory::Allocate(AssetDatabase* db, size_t size, size_t alignment) {
  if (db == nullptr) {
    return nullptr;
  }
  return db->AllocTaskMemory(size, alignment);
}
void TaskMemory::Release(AssetDatabase* db, void* p) {
  if (db == nullptr) {
    return;
  }
  db->FreeTaskMemory(p);
}
AssetDatabase::AssetDatabase()
  : jobsReady(false)
  , taskPool(TaskBlockSize(), 1024, TaskBlockAlign())
  , skyScratch(2 * 1024 * 1024)
  , nextTaskId(1) {
  skyScratch.Initialize();
}
void AssetDatabase::SetAssetsDir(const std::string& dir) {
  assetsDir = dir;
}
const std::string& AssetDatabase::AssetsDir() const {
  return assetsDir;
}
void AssetDatabase::EnsureJobs() {
  if (!jobsReady) {
    jobsReady = jobs.Initialize();
  }
  taskPool.Initialize();
}
void* AssetDatabase::AllocTaskMemory(size_t size, size_t alignment) {
  (void)alignment;
  taskPool.Initialize();
  void* mem = taskPool.Allocate();
  if (mem != nullptr && size <= taskPool.BlockSize()) {
    return mem;
  }
  if (mem != nullptr) {
    taskPool.Free(mem);
  }
  return ::operator new(size, std::nothrow);
}
void AssetDatabase::FreeTaskMemory(void* p) {
  if (p == nullptr) {
    return;
  }
  if (taskPool.Owns(p)) {
    taskPool.Free(p);
    return;
  }
  ::operator delete(p);
}
void AssetDatabase::EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  TaskPtr<ImportMeshTask> task = MakeTask<ImportMeshTask>();
  if (task == nullptr) { return; }
  task->sourcePath = sourcePath;
  task->meshPath = meshPath;
  task->keepGuid = keepGuid;
  task->taskId = nextTaskId++;
  task->outMutex = &completedMutex;
  task->outQueue = &completed;
  jobs.AddTask(task.get());
  pending.push_back(std::move(task));
}
LodBakeTask::LodBakeTask()
  : taskId(0)
  , progress(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void LodBakeTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  progress = 5;
  for (int tier = 1; tier <= 2; ++tier) {
    std::string sibPath = LodSiblingPath(meshPath, tier);
    progress = tier == 1 ? 30 : 65;
    if (sibPath.empty()) {
      continue;
    }
    std::error_code ec;
    if (std::filesystem::exists(sibPath, ec) && !ec) {
      continue;
    }
    MeshAssetData lodMesh;
    float ratio = tier == 1 ? 0.4f : 0.15f;
    float err = tier == 1 ? 0.01f : 0.02f;
    if (GenerateLodMesh(baseData, ratio, err, lodMesh) && EncodeMeshFile(lodMesh, sibPath)) {
      ImportResult result;
      result.ok = true;
      result.taskId = taskId;
      result.sourcePath = sibPath;
      result.meshPath = sibPath;
      result.data = lodMesh;
      if (outMutex != nullptr && outQueue != nullptr) {
        std::lock_guard<std::mutex> lock(*outMutex);
        outQueue->push_back(result);
      }
    }
  }
  progress = 100;
}
void AssetDatabase::EnqueueLodBake(const MeshAssetData& baseData, const std::string& meshPath) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  TaskPtr<LodBakeTask> task = MakeTask<LodBakeTask>();
  if (task == nullptr) { return; }
  task->baseData = baseData;
  task->meshPath = meshPath;
  task->taskId = nextTaskId++;
  task->outMutex = &completedMutex;
  task->outQueue = &completed;
  jobs.AddTask(task.get());
  pendingLod.push_back(std::move(task));
}
MeshRecord* AssetDatabase::FindByGuid(const std::string& guid) {
  auto it = records.find(guid);
  return it == records.end() ? nullptr : &it->second;
}
void AssetDatabase::Scan() {
  relocated.clear();
  if (assetsDir.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::is_directory(assetsDir, ec) || ec) {
    return;
  }
  std::vector<std::string> glbFiles;
  std::vector<std::string> kzmeshFiles;
  std::vector<std::string> imgFiles;
  std::vector<std::string> kztexFiles;
  for (std::filesystem::recursive_directory_iterator it(assetsDir, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    std::string ext = Kizuri::Path::ExtensionLower(it->path().string());
    if (ext == ".glb" || ext == ".gltf") {
      glbFiles.push_back(it->path().string());
    } else if (ext == ".kzmesh") {
      kzmeshFiles.push_back(it->path().string());
    } else if (IsImageExt(ext)) {
      imgFiles.push_back(it->path().string());
    } else if (ext == ".kztex") {
      kztexFiles.push_back(it->path().string());
    }
  }
  for (size_t i = 0; i < kzmeshFiles.size(); ++i) {
    int64_t mtime = 0;
    StatTime(kzmeshFiles[i], mtime);
    auto pg = pathToGuid.find(kzmeshFiles[i]);
    if (pg != pathToGuid.end()) {
      MeshRecord* known = FindByGuid(pg->second);
      auto st = seenMeshTime.find(kzmeshFiles[i]);
      if (known != nullptr && st != seenMeshTime.end() && st->second == mtime) {
        continue;
      }
    }
    auto fd = failedDecode.find(kzmeshFiles[i]);
    if (fd != failedDecode.end() && fd->second == mtime) {
      continue;
    }
    bool flying = false;
    for (size_t k = 0; k < inflight.size(); ++k) {
      if (inflight[k] == kzmeshFiles[i]) {
        flying = true;
        break;
      }
    }
    if (flying || DecodePendingFor(kzmeshFiles[i])) {
      continue;
    }
    EnqueueDecode(kzmeshFiles[i], false);
    inflight.push_back(kzmeshFiles[i]);
  }
  for (size_t i = 0; i < glbFiles.size(); ++i) {
    bool known = false;
    for (auto& kv : records) {
      if (kv.second.hasSource && kv.second.sourcePath == glbFiles[i]) {
        known = true;
        break;
      }
    }
    if (known) {
      continue;
    }
    if (HashPendingForPath(glbFiles[i]) || ImportPendingForPath(glbFiles[i])) {
      continue;
    }
    EnqueueHash(glbFiles[i], "", false);
  }
  for (size_t i = 0; i < kztexFiles.size(); ++i) {
    int64_t mtime = 0;
    StatTime(kztexFiles[i], mtime);
    auto pg = texPathToGuid.find(kztexFiles[i]);
    if (pg != texPathToGuid.end()) {
      auto tr = texRecords.find(pg->second);
      auto st = seenTexTime.find(kztexFiles[i]);
      if (tr != texRecords.end() && st != seenTexTime.end() && st->second == mtime) {
        continue;
      }
    }
    auto fd = failedDecode.find(kztexFiles[i]);
    if (fd != failedDecode.end() && fd->second == mtime) {
      continue;
    }
    if (DecodePendingFor(kztexFiles[i])) {
      continue;
    }
    EnqueueDecode(kztexFiles[i], true);
    inflightTex.push_back(kztexFiles[i]);
  }
  for (size_t i = 0; i < imgFiles.size(); ++i) {
    bool known = false;
    for (auto& kv : texRecords) {
      if (kv.second.hasSource && kv.second.sourcePath == imgFiles[i]) {
        known = true;
        break;
      }
    }
    if (known) {
      continue;
    }
    if (HashPendingForPath(imgFiles[i]) || ImportPendingForPath(imgFiles[i])) {
      continue;
    }
    EnqueueHash(imgFiles[i], "", true);
  }
  for (auto it = pathToGuid.begin(); it != pathToGuid.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      seenMeshTime.erase(it->first);
      it = pathToGuid.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = texPathToGuid.begin(); it != texPathToGuid.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      seenTexTime.erase(it->first);
      it = texPathToGuid.erase(it);
    } else {
      ++it;
    }
  }
  for (auto& kv : records) {
    MeshRecord& record = kv.second;
    if (!record.hasSource) {
      record.state = MeshAssetState::NoSource;
      continue;
    }
    int64_t ticks = 0;
    if (!StatTime(record.sourcePath, ticks)) {
      record.state = MeshAssetState::SourceMissing;
      continue;
    }
    if (ticks != record.lastSeenSourceTime && !HashPendingForPath(record.sourcePath)) {
      EnqueueHash(record.sourcePath, record.guid, false);
    }
  }
  for (auto& kv : texRecords) {
    TextureRecord& record = kv.second;
    if (!record.hasSource) {
      record.state = TextureAssetState::NoSource;
      continue;
    }
    int64_t ticks = 0;
    if (!StatTime(record.sourcePath, ticks)) {
      record.state = TextureAssetState::SourceMissing;
      continue;
    }
    if (ticks != record.lastSeenSourceTime && !HashPendingForPath(record.sourcePath)) {
      EnqueueHash(record.sourcePath, record.guid, true);
    }
  }
  for (auto it = failedDecode.begin(); it != failedDecode.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      it = failedDecode.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = failedImport.begin(); it != failedImport.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      it = failedImport.erase(it);
    } else {
      ++it;
    }
  }
}
ImportTexTask::ImportTexTask()
  : taskId(0)
  , progress(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void ImportTexTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  progress = 10;
  ImportTexResult result;
  result.ok = false;
  result.taskId = taskId;
  result.sourcePath = sourcePath;
  result.texPath = texPath;
  if (sourcePath.empty()) {
    try {
      if (DecodeTextureFile(texPath, result.data) && !result.data.guid.empty()) {
        result.ok = true;
        result.sourcePath = texPath;
      }
    } catch (const std::bad_alloc&) {
      result.error = "out of memory";
    } catch (...) {
      result.error = "decode failed";
    }
  } else {
    try {
      if (ImportTextureFile(sourcePath, keepGuid, result.data)) {
        progress = 60;
        if (EncodeTextureFile(result.data, texPath)) {
          result.ok = true;
        }
      }
    } catch (const std::bad_alloc&) {
      result.error = "out of memory";
    } catch (...) {
      result.error = "import failed";
    }
  }
  progress = 100;
  if (outMutex != nullptr && outQueue != nullptr) {
    std::lock_guard<std::mutex> lock(*outMutex);
    outQueue->push_back(result);
  }
}
HashTask::HashTask()
  : taskId(0)
  , progress(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void HashTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  progress = 50;
  HashResult result;
  result.ok = false;
  result.taskId = taskId;
  result.path = path;
  result.guid = guid;
  result.isTex = isTex;
  result.hash = 0;
  result.fileTime = 0;
  int64_t ticks = 0;
  uint64_t hash = 0;
  if (StatTime(path, ticks) && Fnv1a64File(path, hash)) {
    result.ok = true;
    result.hash = hash;
    result.fileTime = ticks;
  }
  progress = 100;
  if (outMutex != nullptr && outQueue != nullptr) {
    std::lock_guard<std::mutex> lock(*outMutex);
    outQueue->push_back(result);
  }
}
void AssetDatabase::EnqueueDecode(const std::string& compiledPath, bool isTex) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  if (isTex) {
    TaskPtr<ImportTexTask> task = MakeTask<ImportTexTask>();
  if (task == nullptr) { return; }
    task->sourcePath = "";
    task->texPath = compiledPath;
    task->keepGuid = "";
    task->taskId = nextTaskId++;
    task->outMutex = &completedTexMutex;
    task->outQueue = &completedTex;
    jobs.AddTask(task.get());
    pendingTex.push_back(std::move(task));
    inflightTex.push_back(compiledPath);
  } else {
    TaskPtr<ImportMeshTask> task = MakeTask<ImportMeshTask>();
  if (task == nullptr) { return; }
    task->sourcePath = "";
    task->meshPath = compiledPath;
    task->keepGuid = "";
    task->taskId = nextTaskId++;
    task->outMutex = &completedMutex;
    task->outQueue = &completed;
    jobs.AddTask(task.get());
    pending.push_back(std::move(task));
    inflight.push_back(compiledPath);
  }
}
void AssetDatabase::EnqueueHash(const std::string& path, const std::string& guid, bool isTex) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  TaskPtr<HashTask> task = MakeTask<HashTask>();
  if (task == nullptr) { return; }
  task->path = path;
  task->guid = guid;
  task->isTex = isTex;
  task->taskId = nextTaskId++;
  task->outMutex = &completedHashMutex;
  task->outQueue = &completedHash;
  jobs.AddTask(task.get());
  pendingHash.push_back(std::move(task));
}
bool AssetDatabase::HashPendingForPath(const std::string& path) const {
  for (size_t i = 0; i < pendingHash.size(); ++i) {
    if (pendingHash[i]->path == path) {
      return true;
    }
  }
  return false;
}
bool AssetDatabase::ImportPendingForPath(const std::string& path) const {
  for (size_t i = 0; i < inflight.size(); ++i) {
    if (inflight[i] == path) {
      return true;
    }
  }
  for (size_t i = 0; i < inflightTex.size(); ++i) {
    if (inflightTex[i] == path) {
      return true;
    }
  }
  return false;
}
bool AssetDatabase::DecodePendingFor(const std::string& path) const {
  for (size_t i = 0; i < pending.size(); ++i) {
    if (pending[i]->meshPath == path) {
      return true;
    }
  }
  for (size_t i = 0; i < pendingTex.size(); ++i) {
    if (pendingTex[i]->texPath == path) {
      return true;
    }
  }
  return false;
}
bool AssetDatabase::SiblingUsable(const std::string& sourcePath, bool isTex) const {
  std::filesystem::path sib(sourcePath);
  sib.replace_extension(isTex ? ".kztex" : ".kzmesh");
  std::string sibPath = sib.string();
  int64_t ticks = 0;
  if (!StatTime(sibPath, ticks)) {
    return false;
  }
  auto fd = failedDecode.find(sibPath);
  if (fd != failedDecode.end() && fd->second == ticks) {
    return false;
  }
  if (DecodePendingFor(sibPath)) {
    return true;
  }
  if (isTex ? (texPathToGuid.find(sibPath) != texPathToGuid.end()) : (pathToGuid.find(sibPath) != pathToGuid.end())) {
    return true;
  }
  return true;
}
bool AssetDatabase::TryReconnectByHash(const std::string& path, uint64_t hash, int64_t fileTime) {
  for (auto& kv : records) {
    MeshRecord& record = kv.second;
    if (!record.hasSource || record.state != MeshAssetState::SourceMissing || record.sourceHash != hash) {
      continue;
    }
    record.sourcePath = path;
    record.data.sourcePath = path;
    record.lastSeenSourceTime = fileTime;
    EncodeMeshFile(record.data, record.meshPath);
    record.state = MeshAssetState::Ready;
    std::filesystem::path mp(record.meshPath);
    relocated.push_back(mp.filename().string());
    return true;
  }
  for (auto& kv : texRecords) {
    TextureRecord& record = kv.second;
    if (!record.hasSource || record.state != TextureAssetState::SourceMissing || record.sourceHash != hash) {
      continue;
    }
    record.sourcePath = path;
    record.data.sourcePath = path;
    record.lastSeenSourceTime = fileTime;
    EncodeTextureFile(record.data, record.texPath);
    record.state = TextureAssetState::Ready;
    std::filesystem::path mp(record.texPath);
    relocated.push_back(mp.filename().string());
    return true;
  }
  return false;
}
void AssetDatabase::EnqueueTexImport(const std::string& sourcePath, const std::string& texPath, const std::string& keepGuid) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  TaskPtr<ImportTexTask> task = MakeTask<ImportTexTask>();
  if (task == nullptr) { return; }
  task->sourcePath = sourcePath;
  task->texPath = texPath;
  task->keepGuid = keepGuid;
  task->taskId = nextTaskId++;
  task->outMutex = &completedTexMutex;
  task->outQueue = &completedTex;
  jobs.AddTask(task.get());
  pendingTex.push_back(std::move(task));
}
void AssetDatabase::UpsertTexResult(const ImportTexResult& result) {
  if (!result.ok || result.data.guid.empty()) {
    return;
  }
  auto existing = texRecords.find(result.data.guid);
  if (existing != texRecords.end()) {
    DropTexGpu(existing->second);
    existing->second.texPath = result.texPath;
    existing->second.sourcePath = result.data.sourcePath;
    existing->second.sourceHash = result.data.sourceHash;
    existing->second.hasSource = result.data.hasSource;
    existing->second.data = result.data;
    existing->second.loaded = true;
    InitFreshTexState(existing->second);
  } else {
    TextureRecord record;
    record.guid = result.data.guid;
    record.texPath = result.texPath;
    record.sourcePath = result.data.sourcePath;
    record.sourceHash = result.data.sourceHash;
    record.hasSource = result.data.hasSource;
    record.data = result.data;
    record.loaded = true;
    record.gpu = 0;
    record.residentLevels = 0;
    record.selectedLevels = 0;
    InitFreshTexState(record);
    texRecords[record.guid] = record;
  }
  int64_t mtime = 0;
  StatTime(result.texPath, mtime);
  texPathToGuid[result.texPath] = result.data.guid;
  seenTexTime[result.texPath] = mtime;
}
size_t AssetDatabase::DrainCompleted() {
  std::vector<ImportResult> results;
  {
    std::lock_guard<std::mutex> lock(completedMutex);
    results.swap(completed);
  }
  for (size_t i = 0; i < results.size(); ++i) {
    if (!results[i].ok) {
      if (!results[i].error.empty()) {
        std::string name = results[i].sourcePath;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) {
          name = name.substr(slash + 1);
        }
        importErrors.push_back(name + ": " + results[i].error);
      }
      int64_t ticks = 0;
      if (StatTime(results[i].meshPath, ticks)) {
        failedDecode[results[i].meshPath] = ticks;
      }
      if (results[i].sourcePath != results[i].meshPath) {
        int64_t sticks = 0;
        if (StatTime(results[i].sourcePath, sticks)) {
          failedImport[results[i].sourcePath] = sticks;
        }
      }
    } else {
      UpsertResult(results[i]);
      if (results[i].sourcePath != results[i].meshPath && results[i].data.indices.size() >= 6000) {
        EnqueueLodBake(results[i].data, results[i].meshPath);
      }
      if (!results[i].warning.empty()) {
        std::string name = results[i].sourcePath;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) {
          name = name.substr(slash + 1);
        }
        importWarnings.push_back(name + ": " + results[i].warning);
      }
    }
    for (size_t p = 0; p < pending.size(); ++p) {
      if (pending[p]->taskId == results[i].taskId) {
        pending.erase(pending.begin() + p);
        break;
      }
    }
    for (size_t p = 0; p < pendingLod.size(); ++p) {
      if (pendingLod[p]->taskId == results[i].taskId) {
        pendingLod.erase(pendingLod.begin() + p);
        break;
      }
    }
    for (size_t f = 0; f < inflight.size(); ++f) {
      if (inflight[f] == results[i].sourcePath) {
        inflight.erase(inflight.begin() + f);
        break;
      }
    }
  }
  std::vector<ImportTexResult> texResults;
  {
    std::lock_guard<std::mutex> lock(completedTexMutex);
    texResults.swap(completedTex);
  }
  for (size_t i = 0; i < texResults.size(); ++i) {
    if (!texResults[i].ok) {
      if (!texResults[i].error.empty()) {
        std::string name = texResults[i].sourcePath;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) {
          name = name.substr(slash + 1);
        }
        importErrors.push_back(name + ": " + texResults[i].error);
      }
      int64_t ticks = 0;
      if (StatTime(texResults[i].texPath, ticks)) {
        failedDecode[texResults[i].texPath] = ticks;
      }
      if (texResults[i].sourcePath != texResults[i].texPath) {
        int64_t sticks = 0;
        if (StatTime(texResults[i].sourcePath, sticks)) {
          failedImport[texResults[i].sourcePath] = sticks;
        }
      }
    } else {
      UpsertTexResult(texResults[i]);
    }
    for (size_t p = 0; p < pendingTex.size(); ++p) {
      if (pendingTex[p]->taskId == texResults[i].taskId) {
        pendingTex.erase(pendingTex.begin() + p);
        break;
      }
    }
    for (size_t f = 0; f < inflightTex.size(); ++f) {
      if (inflightTex[f] == texResults[i].sourcePath) {
        inflightTex.erase(inflightTex.begin() + f);
        break;
      }
    }
  }
  std::vector<HashResult> hashResults;
  {
    std::lock_guard<std::mutex> lock(completedHashMutex);
    hashResults.swap(completedHash);
  }
  for (size_t i = 0; i < hashResults.size(); ++i) {
    const HashResult& hr = hashResults[i];
    for (size_t p = 0; p < pendingHash.size(); ++p) {
      if (pendingHash[p]->taskId == hr.taskId) {
        pendingHash.erase(pendingHash.begin() + p);
        break;
      }
    }
    if (!hr.ok) {
      continue;
    }
    if (!hr.guid.empty()) {
      if (!hr.isTex) {
        MeshRecord* r = FindByGuid(hr.guid);
        if (r != nullptr && r->sourcePath == hr.path) {
          r->state = (r->sourceHash == hr.hash) ? MeshAssetState::Ready : MeshAssetState::Outdated;
          r->lastSeenSourceTime = hr.fileTime;
        }
      } else {
        auto it = texRecords.find(hr.guid);
        if (it != texRecords.end() && it->second.sourcePath == hr.path) {
          it->second.state = (it->second.sourceHash == hr.hash) ? TextureAssetState::Ready : TextureAssetState::Outdated;
          it->second.lastSeenSourceTime = hr.fileTime;
        }
      }
      continue;
    }
    if (TryReconnectByHash(hr.path, hr.hash, hr.fileTime)) {
      continue;
    }
    bool nowKnown = false;
    if (hr.isTex) {
      for (auto& kv : texRecords) {
        if (kv.second.hasSource && kv.second.sourcePath == hr.path) {
          nowKnown = true;
          break;
        }
      }
    } else {
      for (auto& kv : records) {
        if (kv.second.hasSource && kv.second.sourcePath == hr.path) {
          nowKnown = true;
          break;
        }
      }
    }
    if (nowKnown) {
      continue;
    }
    std::filesystem::path sib(hr.path);
    sib.replace_extension(hr.isTex ? ".kztex" : ".kzmesh");
    if (SiblingUsable(sib.string(), hr.isTex)) {
      continue;
    }
    int64_t smtime = 0;
    if (StatTime(hr.path, smtime)) {
      auto fi = failedImport.find(hr.path);
      if (fi != failedImport.end() && fi->second == smtime) {
        continue;
      }
    }
    if (ImportPendingForPath(hr.path)) {
      continue;
    }
    if (hr.isTex) {
      EnqueueTexImport(hr.path, sib.string(), "");
      inflightTex.push_back(hr.path);
    } else {
      EnqueueImport(hr.path, sib.string(), "");
      inflight.push_back(hr.path);
    }
  }
  return results.size() + texResults.size() + hashResults.size();
}
size_t AssetDatabase::PendingImports() const {
  return pending.size() + pendingTex.size() + pendingLod.size();
}
std::vector<std::pair<std::string, int>> AssetDatabase::ImportingNow() const {
  std::vector<std::pair<std::string, int>> out;
  for (size_t i = 0; i < pending.size(); ++i) {
    std::string name = pending[i]->sourcePath;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    out.push_back(std::make_pair(name, pending[i]->progress.load()));
  }
  for (size_t i = 0; i < pendingTex.size(); ++i) {
    std::string name = pendingTex[i]->sourcePath;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    out.push_back(std::make_pair(name, pendingTex[i]->progress.load()));
  }
  for (size_t i = 0; i < pendingHash.size(); ++i) {
    std::string name = pendingHash[i]->path;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    out.push_back(std::make_pair(name, pendingHash[i]->progress.load()));
  }
  for (size_t i = 0; i < pendingLod.size(); ++i) {
    std::string name = pendingLod[i]->meshPath;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    out.push_back(std::make_pair(name + " LOD", pendingLod[i]->progress.load()));
  }
  return out;
}
void AssetDatabase::DrainBlocking() {
  for (int iter = 0; iter < 50; ++iter) {
    for (size_t i = 0; i < pending.size(); ++i) {
      jobs.WaitForTask(pending[i].get());
    }
    pending.clear();
    for (size_t i = 0; i < pendingTex.size(); ++i) {
      jobs.WaitForTask(pendingTex[i].get());
    }
    pendingTex.clear();
    for (size_t i = 0; i < pendingHash.size(); ++i) {
      jobs.WaitForTask(pendingHash[i].get());
    }
    pendingHash.clear();
    for (size_t i = 0; i < pendingLod.size(); ++i) {
      jobs.WaitForTask(pendingLod[i].get());
    }
    pendingLod.clear();
    DrainCompleted();
    if (PendingImports() == 0) {
      break;
    }
  }
}
void AssetDatabase::InitFreshMeshState(MeshRecord& record) {
  record.lastSeenSourceTime = 0;
  if (record.hasSource) {
    int64_t ticks = 0;
    if (StatTime(record.sourcePath, ticks)) {
      record.lastSeenSourceTime = ticks;
      record.state = MeshAssetState::Ready;
      return;
    }
    record.state = MeshAssetState::SourceMissing;
    return;
  }
  record.state = MeshAssetState::NoSource;
}
void AssetDatabase::InitFreshTexState(TextureRecord& record) {
  record.lastSeenSourceTime = 0;
  if (record.hasSource) {
    int64_t ticks = 0;
    if (StatTime(record.sourcePath, ticks)) {
      record.lastSeenSourceTime = ticks;
      record.state = TextureAssetState::Ready;
      return;
    }
    record.state = TextureAssetState::SourceMissing;
    return;
  }
  record.state = TextureAssetState::NoSource;
}
void AssetDatabase::UpsertResult(const ImportResult& result) {
  if (!result.ok || result.data.guid.empty()) {
    return;
  }
  MeshRecord* existing = FindByGuid(result.data.guid);
  if (existing != nullptr) {
    DropMeshGpu(*existing);
    existing->meshPath = result.meshPath;
    existing->sourcePath = result.data.sourcePath;
    existing->sourceHash = result.data.sourceHash;
    existing->hasSource = result.data.hasSource;
    existing->data = result.data;
    existing->loaded = true;
    InitFreshMeshState(*existing);
  } else {
    MeshRecord record;
    record.guid = result.data.guid;
    record.meshPath = result.meshPath;
    record.sourcePath = result.data.sourcePath;
    record.sourceHash = result.data.sourceHash;
    record.hasSource = result.data.hasSource;
    record.data = result.data;
    record.loaded = true;
    record.gpuVB = 0;
    record.gpuIB = 0;
    record.gpuCount = 0;
    record.gpuReady = false;
    InitFreshMeshState(record);
    records[record.guid] = record;
  }
  int64_t mtime = 0;
  StatTime(result.meshPath, mtime);
  pathToGuid[result.meshPath] = result.data.guid;
  seenMeshTime[result.meshPath] = mtime;
}
const MeshRecord* AssetDatabase::GetByGuid(const std::string& guid) const {
  auto it = records.find(guid);
  return it == records.end() ? nullptr : &it->second;
}
const MeshRecord* AssetDatabase::GetByMeshPath(const std::string& path) const {
  for (auto& kv : records) {
    if (kv.second.meshPath == path) {
      return &kv.second;
    }
  }
  return nullptr;
}
std::vector<std::string> AssetDatabase::AllGuids() const {
  std::vector<std::string> out;
  for (auto& kv : records) {
    out.push_back(kv.first);
  }
  return out;
}
const TextureRecord* AssetDatabase::GetTexByGuid(const std::string& guid) const {
  auto it = texRecords.find(guid);
  return it == texRecords.end() ? nullptr : &it->second;
}
std::vector<std::string> AssetDatabase::AllTexGuids() const {
  std::vector<std::string> out;
  for (auto& kv : texRecords) {
    out.push_back(kv.first);
  }
  return out;
}
bool AssetDatabase::Reimport(const std::string& guid) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || !record->hasSource) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(record->sourcePath, ec) || ec) {
    return false;
  }
  EnqueueImport(record->sourcePath, record->meshPath, guid);
  return true;
}
bool AssetDatabase::ReimportTexture(const std::string& guid) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || !it->second.hasSource) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(it->second.sourcePath, ec) || ec) {
    return false;
  }
  EnqueueTexImport(it->second.sourcePath, it->second.texPath, guid);
  return true;
}
size_t AssetDatabase::RelocateMissing() {
  if (assetsDir.empty()) {
    return 0;
  }
  std::error_code ec;
  size_t queued = 0;
  for (std::filesystem::recursive_directory_iterator it(assetsDir, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    std::string ext = Kizuri::Path::ExtensionLower(it->path().string());
    bool isTex = IsImageExt(ext);
    if (ext != ".glb" && ext != ".gltf" && !isTex) {
      continue;
    }
    std::string file = it->path().string();
    bool referenced = false;
    for (auto& kv : records) {
      if (kv.second.hasSource && kv.second.sourcePath == file) {
        referenced = true;
        break;
      }
    }
    if (!referenced) {
      for (auto& kv : texRecords) {
        if (kv.second.hasSource && kv.second.sourcePath == file) {
          referenced = true;
          break;
        }
      }
    }
    if (referenced || HashPendingForPath(file)) {
      continue;
    }
    EnqueueHash(file, "", isTex);
    ++queued;
  }
  return queued;
}
std::vector<std::string> AssetDatabase::TakeRelocated() {
  std::vector<std::string> out = relocated;
  relocated.clear();
  return out;
}
std::vector<std::string> AssetDatabase::TakeImportErrors() {
  std::vector<std::string> out = importErrors;
  importErrors.clear();
  return out;
}
std::vector<std::string> AssetDatabase::TakeImportWarnings() {
  std::vector<std::string> out = importWarnings;
  importWarnings.clear();
  return out;
}
bool AssetDatabase::RenameAssetFile(const std::string& guid, const std::string& newFileName) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || newFileName.empty()) {
    return false;
  }
  std::filesystem::path oldPath(record->meshPath);
  std::filesystem::path target = oldPath.parent_path() / newFileName;
  if (target.extension().string() != ".kzmesh") {
    target.replace_extension(".kzmesh");
  }
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) {
    return false;
  }
  std::filesystem::rename(oldPath, target, ec);
  if (ec) {
    return false;
  }
  record->meshPath = target.string();
  return true;
}
bool AssetDatabase::DeleteAssetFile(const std::string& guid, bool force, const std::vector<RefUse>& refs, std::vector<std::string>& blockedBy) {
  blockedBy.clear();
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr) {
    return false;
  }
  for (size_t i = 0; i < refs.size(); ++i) {
    if (refs[i].assetGuid == guid) {
      blockedBy.push_back(refs[i].userLabel);
    }
  }
  if (!blockedBy.empty() && !force) {
    return false;
  }
  DropMeshGpu(*record);
  std::error_code ec;
  std::filesystem::remove(record->meshPath, ec);
  records.erase(guid);
  return true;
}
std::map<std::string, size_t> AssetDatabase::ComputeRefCounts(const std::vector<RefUse>& refs) const {
  std::map<std::string, size_t> counts;
  for (size_t i = 0; i < refs.size(); ++i) {
    if (!refs[i].assetGuid.empty()) {
      counts[refs[i].assetGuid]++;
    }
  }
  return counts;
}
bool AssetDatabase::RenameTextureFile(const std::string& guid, const std::string& newFileName) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || newFileName.empty()) {
    return false;
  }
  std::filesystem::path oldPath(it->second.texPath);
  std::filesystem::path target = oldPath.parent_path() / newFileName;
  if (target.extension().string() != ".kztex") {
    target.replace_extension(".kztex");
  }
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) {
    return false;
  }
  std::filesystem::rename(oldPath, target, ec);
  if (ec) {
    return false;
  }
  it->second.texPath = target.string();
  return true;
}
bool AssetDatabase::DeleteTextureFile(const std::string& guid, bool force, std::vector<std::string>& blockedBy) {
  blockedBy.clear();
  auto it = texRecords.find(guid);
  if (it == texRecords.end()) {
    return false;
  }
  for (auto& kv : records) {
    for (size_t m = 0; m < kv.second.data.materials.size(); ++m) {
      if (kv.second.data.materials[m].albedoTexGuid == guid) {
        std::filesystem::path mp(kv.second.meshPath);
        blockedBy.push_back(mp.filename().string());
        break;
      }
    }
  }
  if (!blockedBy.empty() && !force) {
    return false;
  }
  DropTexGpu(it->second);
  std::error_code ec;
  std::filesystem::remove(it->second.texPath, ec);
  texRecords.erase(it);
  return true;
}
std::map<std::string, size_t> AssetDatabase::ComputeTexRefCounts() const {
  std::map<std::string, size_t> counts;
  for (auto& kv : records) {
    for (size_t m = 0; m < kv.second.data.materials.size(); ++m) {
      const std::string& tg = kv.second.data.materials[m].albedoTexGuid;
      if (!tg.empty()) {
        counts[tg]++;
      }
    }
  }
  return counts;
}
int AssetDatabase::SelectMipLevel(float dist, int mipCount) {
  if (mipCount <= 0) {
    return 0;
  }
  if (dist < 0.0f) {
    dist = 0.0f;
  }
  int want = mipCount;
  if (dist < 15.0f) {
    want = mipCount;
  } else if (dist < 40.0f) {
    want = mipCount > 1 ? mipCount - 1 : 1;
    if (want < 1) {
      want = 1;
    }
  } else if (dist < 100.0f) {
    want = 2;
  } else {
    want = 1;
  }
  if (want > mipCount) {
    want = mipCount;
  }
  if (want < 1) {
    want = 1;
  }
  return want;
}
void AssetDatabase::UpdateStreaming(const float cameraPos[3], const std::vector<MeshUse>& uses) {
  for (size_t u = 0; u < uses.size(); ++u) {
    auto mr = records.find(uses[u].meshGuid);
    if (mr == records.end() || !mr->second.loaded) {
      continue;
    }
    float dx = uses[u].pos[0] - cameraPos[0];
    float dy = uses[u].pos[1] - cameraPos[1];
    float dz = uses[u].pos[2] - cameraPos[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    bool isBatch = uses[u].meshGuid.compare(0, 8, "batch://") == 0;
    if (!isBatch && meshStreaming && !uses[u].tierOnly && dist > meshStreamDist && mr->second.gpuReady) {
      DropMeshGpu(mr->second);
      ++streamEvictions;
    }
    if (!isBatch && geoStreaming && uses[u].tierOnly && dist > geoStreamDist && mr->second.gpuReady) {
      DropMeshGpu(mr->second);
      ++streamEvictions;
    }
    for (size_t m = 0; m < mr->second.data.materials.size(); ++m) {
      const MeshMaterialData& mm = mr->second.data.materials[m];
      const std::string* slots[17] = { &mm.albedoTexGuid, &mm.normalTexGuid, &mm.ormTexGuid, &mm.heightTexGuid, &mm.emissiveTexGuid, &mm.specularTexGuid, &mm.specularColorTexGuid, &mm.clearcoatTexGuid, &mm.clearcoatRoughnessTexGuid, &mm.clearcoatNormalTexGuid, &mm.sheenColorTexGuid, &mm.sheenRoughnessTexGuid, &mm.anisotropyTexGuid, &mm.iridescenceTexGuid, &mm.iridescenceThicknessTexGuid, &mm.transmissionTexGuid, &mm.thicknessTexGuid };
      for (int s = 0; s < 17; ++s) {
        RequestTexture(*slots[s], dist);
      }
    }
  }
}
void AssetDatabase::RequestTexture(const std::string& guid, float dist) {
  if (guid.empty()) {
    return;
  }
  if (vtIndex.find(guid) != vtIndex.end()) {
    return;
  }
  auto tr = texRecords.find(guid);
  if (tr == texRecords.end() || !tr->second.loaded) {
    return;
  }
  if (dist < 0.0f) {
    dist = 0.0f;
  }
  int want = SelectMipLevel(dist, static_cast<int>(tr->second.data.mips.size()));
  if (want <= tr->second.selectedLevels) {
    return;
  }
  tr->second.selectedLevels = want;
  for (int mip = tr->second.residentLevels; mip < want; ++mip) {
    bool queued = false;
    for (size_t q = 0; q < uploadQueue.size(); ++q) {
      if (uploadQueue[q].guid == guid && uploadQueue[q].mip == mip) {
        queued = true;
        break;
      }
    }
    if (!queued) {
      TexUpload up;
      up.guid = guid;
      up.mip = mip;
      uploadQueue.push_back(up);
    }
  }
}
void AssetDatabase::SetMeshStreaming(bool on, float distance) {
  meshStreaming = on;
  if (distance > 5.0f) {
    meshStreamDist = distance > 2000.0f ? 2000.0f : distance;
  }
}
void AssetDatabase::SetGeometryStreaming(bool on, float distance) {
  geoStreaming = on;
  if (distance > 5.0f) {
    geoStreamDist = distance > 2000.0f ? 2000.0f : distance;
  }
}
void AssetDatabase::UpsertBatchMesh(const std::string& key, const MeshAssetData& data) {
  if (key.empty() || data.positions.empty() || data.indices.empty()) {
    return;
  }
  auto it = records.find(key);
  if (it != records.end() && it->second.gpuReady) {
    return;
  }
  if (it != records.end()) {
    DropMeshGpu(it->second);
    it->second.data = data;
    it->second.loaded = true;
    return;
  }
  MeshRecord rec;
  rec.guid = key;
  rec.meshPath = "";
  rec.sourcePath = "";
  rec.sourceHash = 0;
  rec.hasSource = false;
  rec.state = MeshAssetState::NoSource;
  rec.data = data;
  rec.loaded = true;
  rec.lastSeenSourceTime = 0;
  rec.gpuVB = 0;
  rec.gpuIB = 0;
  rec.gpuCount = 0;
  rec.gpuReady = false;
  rec.stageVBDone = 0;
  rec.stageIBDone = 0;
  records[key] = rec;
}
void AssetDatabase::DiscardBatchMeshes() {
  for (auto it = records.begin(); it != records.end();) {
    if (it->first.compare(0, 8, "batch://") == 0) {
      DropMeshGpu(it->second);
      it = records.erase(it);
    } else {
      ++it;
    }
  }
}
static DXGI_FORMAT CookieDxgiFormat(TexFormat fmt) {
  switch (fmt) {
    case TexFormat::Rgba8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TexFormat::Bc1:
      return DXGI_FORMAT_BC1_UNORM;
    case TexFormat::Bc3:
      return DXGI_FORMAT_BC3_UNORM;
    case TexFormat::Bc5:
      return DXGI_FORMAT_BC5_UNORM;
    case TexFormat::Bc4:
      return DXGI_FORMAT_BC4_UNORM;
    case TexFormat::Bc6H:
      return DXGI_FORMAT_BC6H_UF16;
    case TexFormat::Bc7:
      return DXGI_FORMAT_BC7_UNORM;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}
RHITexture AssetDatabase::BuildCookieArray(const std::vector<const TextureAssetData*>& slices) {  if (gpuRhi == nullptr) {
    return 0;
  }
  size_t n = slices.size();
  if (n == 0) {
    n = 1;
  }
  if (n > 16) {
    n = 16;
  }
  if (cookieArrayTex != 0) {
    gpuRhi->DestroyTexture(cookieArrayTex);
    cookieArrayTex = 0;
  }
  RHITexture arr = gpuRhi->CreateTextureArray(256, 256, static_cast<int>(n), 1, RHITextureFormat::RGBA8_UNORM);
  if (arr == 0) {
    return 0;
  }
  std::vector<unsigned char> white(static_cast<size_t>(256) * 256 * 4, 255);
  for (size_t s = 0; s < n; ++s) {
    const unsigned char* bytes = white.data();
    uint32_t pitch = 1024;
    size_t byteCount = white.size();
    DirectX::ScratchImage owned;
    const DirectX::Image* img = nullptr;
    DirectX::ScratchImage decompressed;
    DirectX::ScratchImage resized;
    const TextureAssetData* td = s < slices.size() ? slices[s] : nullptr;
    if (td != nullptr && !td->mips.empty()) {
      const TextureMipData& mip = td->mips[0];
      DXGI_FORMAT srcFmt = CookieDxgiFormat(td->format);
      if (mip.width > 0 && mip.height > 0 && !mip.data.empty() && srcFmt != DXGI_FORMAT_UNKNOWN) {
        DirectX::TexMetadata md = {};
        md.width = mip.width;
        md.height = mip.height;
        md.depth = 1;
        md.arraySize = 1;
        md.mipLevels = 1;
        md.miscFlags = 0;
        md.miscFlags2 = 0;
        md.format = srcFmt;
        md.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
        DirectX::Image srcImg = {};
        srcImg.width = mip.width;
        srcImg.height = mip.height;
        srcImg.format = srcFmt;
        srcImg.rowPitch = mip.rowPitch;
        srcImg.slicePitch = mip.data.size();
        srcImg.pixels = const_cast<uint8_t*>(mip.data.data());
        img = &srcImg;
        if (srcFmt != DXGI_FORMAT_R8G8B8A8_UNORM) {
          if (FAILED(DirectX::Decompress(&srcImg, 1, md, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, decompressed))) {
            img = nullptr;
          } else {
            img = decompressed.GetImage(0, 0, 0);
          }
        }
        if (img != nullptr && (img->width != 256 || img->height != 256)) {
          DirectX::TexMetadata rmd = {};
          rmd.width = img->width;
          rmd.height = img->height;
          rmd.depth = 1;
          rmd.arraySize = 1;
          rmd.mipLevels = 1;
          rmd.miscFlags = 0;
          rmd.miscFlags2 = 0;
          rmd.format = DXGI_FORMAT_R8G8B8A8_UNORM;
          rmd.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
          if (FAILED(DirectX::Resize(img, 1, rmd, 256, 256, DirectX::TEX_FILTER_DEFAULT, resized))) {
            img = nullptr;
          } else {
            img = resized.GetImage(0, 0, 0);
          }
        }
        if (img != nullptr && img->width == 256 && img->height == 256 && img->pixels != nullptr) {
          bytes = img->pixels;
          pitch = static_cast<uint32_t>(img->rowPitch);
          byteCount = img->slicePitch;
        } else {
          bytes = white.data();
          pitch = 1024;
          byteCount = white.size();
          img = nullptr;
        }
      }
    }
    if (!gpuRhi->UpdateTextureArraySlice(arr, static_cast<int>(s), 0, 256, 256, pitch, bytes, byteCount)) {
      gpuRhi->DestroyTexture(arr);
      return 0;
    }
  }
  cookieArrayTex = arr;
  return arr;
}
static bool VtMipToRgba(const TextureAssetData& td, int mip, std::vector<unsigned char>& rgbaOut, int& wOut, int& hOut) {
  rgbaOut.clear();
  wOut = 0;
  hOut = 0;
  if (mip < 0 || mip >= static_cast<int>(td.mips.size())) {
    return false;
  }
  const TextureMipData& m = td.mips[mip];
  DXGI_FORMAT srcFmt = CookieDxgiFormat(td.format);
  if (m.width == 0 || m.height == 0 || m.data.empty() || srcFmt == DXGI_FORMAT_UNKNOWN) {
    return false;
  }
  if (m.rowPitch < m.width * 4 && srcFmt == DXGI_FORMAT_R8G8B8A8_UNORM) {
    return false;
  }
  if (srcFmt == DXGI_FORMAT_R8G8B8A8_UNORM) {
    wOut = static_cast<int>(m.width);
    hOut = static_cast<int>(m.height);
    rgbaOut.resize(static_cast<size_t>(m.width) * m.height * 4);
    for (uint32_t y = 0; y < m.height; ++y) {
      std::memcpy(&rgbaOut[static_cast<size_t>(y) * m.width * 4], &m.data[static_cast<size_t>(y) * m.rowPitch], static_cast<size_t>(m.width) * 4);
    }
    return true;
  }
  DirectX::TexMetadata md = {};
  md.width = m.width;
  md.height = m.height;
  md.depth = 1;
  md.arraySize = 1;
  md.mipLevels = 1;
  md.miscFlags = 0;
  md.miscFlags2 = 0;
  md.format = srcFmt;
  md.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
  DirectX::Image srcImg = {};
  srcImg.width = m.width;
  srcImg.height = m.height;
  srcImg.format = srcFmt;
  srcImg.rowPitch = m.rowPitch;
  srcImg.slicePitch = m.data.size();
  srcImg.pixels = const_cast<uint8_t*>(m.data.data());
  DirectX::ScratchImage dec;
  if (FAILED(DirectX::Decompress(&srcImg, 1, md, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, dec))) {
    return false;
  }
  const DirectX::Image* di = dec.GetImage(0, 0, 0);
  if (di == nullptr || di->pixels == nullptr) {
    return false;
  }
  wOut = static_cast<int>(di->width);
  hOut = static_cast<int>(di->height);
  rgbaOut.resize(di->slicePitch);
  std::memcpy(rgbaOut.data(), di->pixels, di->slicePitch);
  return true;
}
int AssetDatabase::CreateVirtualTexture(const TextureAssetData* src, const std::string& texGuid) {
  if (src == nullptr || texGuid.empty() || gpuRhi == nullptr) {
    return -1;
  }
  auto existing = vtIndex.find(texGuid);
  if (existing != vtIndex.end()) {
    return static_cast<int>(existing->second);
  }
  if (src->width == 0 || src->height == 0 || src->width > 1024 || src->height > 1024 || src->mips.empty()) {
    return -1;
  }
  VtEntry e;
  e.texGuid = texGuid;
  e.w = static_cast<int>(src->width);
  e.h = static_cast<int>(src->height);
  e.mipCount = static_cast<int>(src->mips.size());
  e.pagesX = (e.w + 31) / 32;
  e.pagesY = (e.h + 31) / 32;
  if (e.pagesX * e.pagesY > 1024) {
    return -1;
  }
  e.physical = gpuRhi->CreateTexture2D(1024, 1024, 1, RHITextureFormat::RGBA8_UNORM);
  e.tableTex = gpuRhi->CreateTexture2D(64, 64, 1, RHITextureFormat::RGBA8_UNORM);
  e.tailTex = gpuRhi->CreateTexture2D(64, 64, 1, RHITextureFormat::RGBA8_UNORM);
  if (e.physical == 0 || e.tableTex == 0 || e.tailTex == 0) {
    if (e.physical != 0) {
      gpuRhi->DestroyTexture(e.physical);
    }
    if (e.tableTex != 0) {
      gpuRhi->DestroyTexture(e.tableTex);
    }
    if (e.tailTex != 0) {
      gpuRhi->DestroyTexture(e.tailTex);
    }
    return -1;
  }
  std::vector<unsigned char> black(static_cast<size_t>(1024) * 1024 * 4, 0);
  gpuRhi->UpdateTextureMip(e.physical, 0, 1024, 1024, 4096, black.data(), black.size());
  e.tableCpu.assign(static_cast<size_t>(64) * 64 * 4, 0);
  gpuRhi->UpdateTextureMip(e.tableTex, 0, 64, 64, 256, e.tableCpu.data(), e.tableCpu.size());
  std::vector<unsigned char> tailRgba;
  int tailW = 0;
  int tailH = 0;
  bool tailOk = VtMipToRgba(*src, e.mipCount - 1, tailRgba, tailW, tailH) && tailW > 0 && tailH > 0;
  std::vector<unsigned char> tail64(static_cast<size_t>(64) * 64 * 4, 0);
  if (tailOk) {
    for (int y = 0; y < 64; ++y) {
      int sy = y * tailH / 64;
      for (int x = 0; x < 64; ++x) {
        int sx = x * tailW / 64;
        size_t di = (static_cast<size_t>(y) * 64 + x) * 4;
        size_t si = (static_cast<size_t>(sy) * tailW + sx) * 4;
        tail64[di + 0] = tailRgba[si + 0];
        tail64[di + 1] = tailRgba[si + 1];
        tail64[di + 2] = tailRgba[si + 2];
        tail64[di + 3] = tailRgba[si + 3];
      }
    }
  }
  gpuRhi->UpdateTextureMip(e.tailTex, 0, 64, 64, 256, tail64.data(), tail64.size());
  e.residentMip = 99;
  e.nextPage = 0;
  vtEntries.push_back(e);
  vtIndex[texGuid] = vtEntries.size() - 1;
  return static_cast<int>(vtEntries.size() - 1);
}
bool AssetDatabase::UpdateVirtualTexture(const std::string& texGuid, float dist, int budget) {
  auto it = vtIndex.find(texGuid);
  if (it == vtIndex.end() || gpuRhi == nullptr) {
    return false;
  }
  VtEntry& e = vtEntries[it->second];
  auto tr = texRecords.find(texGuid);
  if (tr == texRecords.end() || !tr->second.loaded) {
    return false;
  }
  const TextureAssetData& td = tr->second.data;
  if (budget < 1) {
    budget = 1;
  }
  if (budget > 64) {
    budget = 64;
  }
  int want = e.mipCount - SelectMipLevel(dist, e.mipCount);
  if (want < 0) {
    want = 0;
  }
  if (want >= e.mipCount) {
    want = e.mipCount - 1;
  }
  if (want < 0) {
    return false;
  }
  if (want > 5) {
    want = 99;
  }
  if (want != e.residentMip) {
    e.residentMip = want;
    e.nextPage = 0;
    e.mipRgbaMip = -1;
    e.mipRgba.clear();
    std::fill(e.tableCpu.begin(), e.tableCpu.end(), 0);
    gpuRhi->UpdateTextureMip(e.tableTex, 0, 64, 64, 256, e.tableCpu.data(), e.tableCpu.size());
    if (want == 99) {
      return true;
    }
  }
  if (want == 99) {
    return true;
  }
  if (e.mipRgbaMip != want) {
    if (!VtMipToRgba(td, want, e.mipRgba, e.mipRgbaW, e.mipRgbaH) || e.mipRgbaW <= 0 || e.mipRgbaH <= 0) {
      return false;
    }
    e.mipRgbaMip = want;
  }
  int total = e.pagesX * e.pagesY;
  int step = 32 >> want;
  if (step < 1) {
    step = 1;
  }
  int done = 0;
  while (done < budget && e.nextPage < total) {
    int p = e.nextPage++;
    int px = p % e.pagesX;
    int py = p / e.pagesX;
    int sx = px * step;
    int sy = py * step;
    if (sx >= e.mipRgbaW || sy >= e.mipRgbaH) {
      continue;
    }
    int sw = step;
    int sh = step;
    if (sx + sw > e.mipRgbaW) {
      sw = e.mipRgbaW - sx;
    }
    if (sy + sh > e.mipRgbaH) {
      sh = e.mipRgbaH - sy;
    }
    std::vector<unsigned char> tile(static_cast<size_t>(32) * 32 * 4);
    for (int y = 0; y < 32; ++y) {
      int ryy = y * sh / 32;
      for (int x = 0; x < 32; ++x) {
        int rxx = x * sw / 32;
        size_t di = (static_cast<size_t>(y) * 32 + x) * 4;
        size_t si = (static_cast<size_t>(sy + ryy) * e.mipRgbaW + (sx + rxx)) * 4;
        tile[di + 0] = e.mipRgba[si + 0];
        tile[di + 1] = e.mipRgba[si + 1];
        tile[di + 2] = e.mipRgba[si + 2];
        tile[di + 3] = e.mipRgba[si + 3];
      }
    }
    if (!gpuRhi->UpdateTextureRegion(e.physical, 0, (p % 32) * 32, (p / 32) * 32, 32, 32, 128, tile.data(), tile.size())) {
      return false;
    }
    size_t ti = (static_cast<size_t>(py) * 64 + px) * 4;
    e.tableCpu[ti + 0] = static_cast<unsigned char>(p % 32);
    e.tableCpu[ti + 1] = static_cast<unsigned char>(p / 32);
    e.tableCpu[ti + 2] = 255;
    e.tableCpu[ti + 3] = static_cast<unsigned char>(want > 255 ? 255 : want);
    ++done;
  }
  if (done > 0) {
    gpuRhi->UpdateTextureMip(e.tableTex, 0, 64, 64, 256, e.tableCpu.data(), e.tableCpu.size());
  }
  return true;
}
bool AssetDatabase::GetVirtualBundle(const std::string& texGuid, VtBundle& out) {
  out = VtBundle();
  auto it = vtIndex.find(texGuid);
  if (it == vtIndex.end()) {
    return false;
  }
  const VtEntry& e = vtEntries[it->second];
  if (e.physical == 0 || e.tableTex == 0 || e.tailTex == 0) {
    return false;
  }
  out.physical = e.physical;
  out.table = e.tableTex;
  out.tail = e.tailTex;
  out.residentMip = static_cast<float>(e.residentMip);
  out.valid = true;
  return true;
}
std::string AssetDatabase::CreateProceduralTexture(int mode, int w, int h) {
  if (mode < 0 || mode > 3) {
    return "";
  }
  if (w < 16) {
    w = 16;
  }
  if (h < 16) {
    h = 16;
  }
  if (w > 512) {
    w = 512;
  }
  if (h > 512) {
    h = 512;
  }
  char guidBuf[64];
  std::snprintf(guidBuf, sizeof(guidBuf), "proc://%d_%dx%d", mode, w, h);
  std::string guid = guidBuf;
  auto existing = texRecords.find(guid);
  if (existing != texRecords.end() && existing->second.loaded) {
    return guid;
  }
  std::vector<unsigned char> base(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      size_t o = (static_cast<size_t>(y) * w + x) * 4;
      if (mode == 0) {
        bool c = ((x / 8) + (y / 8)) % 2 == 0;
        base[o + 0] = c ? 255 : 32;
        base[o + 1] = c ? 255 : 32;
        base[o + 2] = c ? 255 : 32;
        base[o + 3] = 255;
      } else if (mode == 1) {
        uint32_t hx = static_cast<uint32_t>(x * 1973 + y * 9277 + 26699u) | 1u;
        hx ^= hx >> 13;
        hx *= 1274126177u;
        hx ^= hx >> 16;
        unsigned char v = static_cast<unsigned char>((hx >> 8) & 255u);
        base[o + 0] = v;
        base[o + 1] = v;
        base[o + 2] = v;
        base[o + 3] = 255;
      } else if (mode == 2) {
        unsigned char v = static_cast<unsigned char>(y * 255 / (h > 1 ? h - 1 : 1));
        base[o + 0] = v;
        base[o + 1] = static_cast<unsigned char>(255 - v);
        base[o + 2] = static_cast<unsigned char>(128 + v / 2);
        base[o + 3] = 255;
      } else {
        base[o + 0] = 128;
        base[o + 1] = 128;
        base[o + 2] = 255;
        base[o + 3] = 255;
      }
    }
  }
  DirectX::Image srcImg = {};
  srcImg.width = static_cast<size_t>(w);
  srcImg.height = static_cast<size_t>(h);
  srcImg.format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srcImg.rowPitch = static_cast<size_t>(w) * 4;
  srcImg.slicePitch = base.size();
  srcImg.pixels = base.data();
  DirectX::TexMetadata md = {};
  md.width = static_cast<size_t>(w);
  md.height = static_cast<size_t>(h);
  md.depth = 1;
  md.arraySize = 1;
  md.mipLevels = 1;
  md.miscFlags = 0;
  md.miscFlags2 = 0;
  md.format = DXGI_FORMAT_R8G8B8A8_UNORM;
  md.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
  DirectX::ScratchImage mipChain;
  if (FAILED(DirectX::GenerateMipMaps(&srcImg, 1, md, DirectX::TEX_FILTER_DEFAULT, 0, mipChain))) {
    return "";
  }
  TextureRecord rec;
  rec.guid = guid;
  rec.texPath = guid;
  rec.sourcePath = "";
  rec.sourceHash = 0;
  rec.hasSource = false;
  rec.state = TextureAssetState::Ready;
  rec.data.guid = guid;
  rec.data.width = static_cast<uint32_t>(w);
  rec.data.height = static_cast<uint32_t>(h);
  rec.data.format = TexFormat::Rgba8;
  rec.data.srgb = mode != 3;
  rec.data.hasSource = false;
  rec.data.sourcePath = "";
  rec.data.sourceHash = 0;
  rec.data.sourceTimestamp = 0;
  size_t mipCount = mipChain.GetImageCount();
  for (size_t m = 0; m < mipCount; ++m) {
    const DirectX::Image* img = mipChain.GetImage(m, 0, 0);
    if (img == nullptr || img->pixels == nullptr) {
      return "";
    }
    TextureMipData mip;
    mip.width = static_cast<uint32_t>(img->width);
    mip.height = static_cast<uint32_t>(img->height);
    mip.rowPitch = static_cast<uint32_t>(img->rowPitch);
    mip.data.assign(img->pixels, img->pixels + img->slicePitch);
    rec.data.mips.push_back(mip);
  }
  if (rec.data.mips.empty()) {
    return "";
  }
  rec.loaded = true;
  rec.lastSeenSourceTime = 0;
  rec.gpu = 0;
  rec.residentLevels = 0;
  rec.selectedLevels = 0;
  texRecords[guid] = rec;
  return guid;
}
RHITexture AssetDatabase::BuildDecalAtlas(const std::vector<const TextureAssetData*>& textures, std::vector<float>& outRects) {
  outRects.clear();
  if (gpuRhi == nullptr || textures.empty()) {
    return 0;
  }
  size_t n = textures.size();
  if (n > 16) {
    n = 16;
  }
  if (decalAtlasTex != 0) {
    gpuRhi->DestroyTexture(decalAtlasTex);
    decalAtlasTex = 0;
  }
  RHITexture arr = gpuRhi->CreateTexture2D(2048, 2048, 1, RHITextureFormat::RGBA8_UNORM);
  if (arr == 0) {
    return 0;
  }
  std::vector<unsigned char> black(static_cast<size_t>(2048) * 2048 * 4, 0);
  gpuRhi->UpdateTextureMip(arr, 0, 2048, 2048, 8192, black.data(), black.size());
  outRects.reserve(n * 4);
  for (size_t s = 0; s < n; ++s) {
    int cellX = static_cast<int>(s % 4);
    int cellY = static_cast<int>(s / 4);
    float u0 = static_cast<float>(cellX * 512) / 2048.0f;
    float v0 = static_cast<float>(cellY * 512) / 2048.0f;
    float u1 = static_cast<float>(cellX * 512 + 512) / 2048.0f;
    float v1 = static_cast<float>(cellY * 512 + 512) / 2048.0f;
    outRects.push_back(u0);
    outRects.push_back(v0);
    outRects.push_back(u1);
    outRects.push_back(v1);
    const TextureAssetData* td = textures[s];
    std::vector<unsigned char> rgba;
    int rw = 0;
    int rh = 0;
    if (td == nullptr || !VtMipToRgba(*td, 0, rgba, rw, rh) || rw <= 0 || rh <= 0) {
      continue;
    }
    std::vector<unsigned char> cell(static_cast<size_t>(512) * 512 * 4);
    for (int y = 0; y < 512; ++y) {
      int sy = y * rh / 512;
      for (int x = 0; x < 512; ++x) {
        int sx = x * rw / 512;
        size_t di = (static_cast<size_t>(y) * 512 + x) * 4;
        size_t si = (static_cast<size_t>(sy) * rw + sx) * 4;
        cell[di + 0] = rgba[si + 0];
        cell[di + 1] = rgba[si + 1];
        cell[di + 2] = rgba[si + 2];
        cell[di + 3] = rgba[si + 3];
      }
    }
    gpuRhi->UpdateTextureRegion(arr, 0, cellX * 512, cellY * 512, 512, 512, 2048, cell.data(), cell.size());
  }
  decalAtlasTex = arr;
  return arr;
}
bool AssetDatabase::EnsureTextureKind(const std::string& guid, TexKind kind) {
  if (guid.empty() || (kind != TexKind::Normal && kind != TexKind::Orm)) {
    return false;
  }
  auto tr = texRecords.find(guid);
  if (tr == texRecords.end() || !tr->second.loaded) {
    return true;
  }
  TextureRecord& rec = tr->second;
  bool wantNormal = kind == TexKind::Normal;
  bool kindOk = false;
  if (wantNormal) {
    kindOk = rec.data.format == TexFormat::Bc5;
  } else if (!rec.data.srgb) {
    kindOk = rec.data.format == TexFormat::Bc1 || rec.data.format == TexFormat::Bc3;
  }
  if (kindOk) {
    return true;
  }
  if (!rec.hasSource || rec.sourcePath.empty()) {
    return false;
  }
  TextureAssetData fixed;
  if (!ImportTextureFileAs(rec.sourcePath, rec.guid, fixed, kind)) {
    return false;
  }
  if (!EncodeTextureFile(fixed, rec.texPath)) {
    return false;
  }
  DropTexGpu(rec);
  rec.data = fixed;
  rec.loaded = true;
  rec.state = TextureAssetState::Ready;
  rec.residentLevels = 0;
  rec.selectedLevels = 0;
  return true;
}
size_t AssetDatabase::DrainUploads(IRHI* rhi, size_t maxBytes) {
  if (rhi == nullptr || maxBytes == 0) {
    return 0;
  }
  size_t done = 0;
  size_t used = 0;
  while (!uploadQueue.empty()) {
    TexUpload up = uploadQueue.front();
    auto tr = texRecords.find(up.guid);
    if (tr == texRecords.end() || !tr->second.loaded) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    TextureRecord& rec = tr->second;
    if (up.mip < 0 || up.mip >= static_cast<int>(rec.data.mips.size())) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    if (rec.gpu == 0) {
      rec.gpu = rhi->CreateTexture2D(static_cast<int>(rec.data.width), static_cast<int>(rec.data.height), static_cast<int>(rec.data.mips.size()), ToRHIFormat(rec.data.format, rec.data.srgb));
      rec.residentLevels = 0;
      if (rec.gpu == 0) {
        uploadQueue.erase(uploadQueue.begin());
        continue;
      }
    }
    const TextureMipData& mip = rec.data.mips[static_cast<size_t>(up.mip)];
    size_t cost = mip.data.size();
    if (used + cost > maxBytes && done > 0) {
      break;
    }
    if (!rhi->UpdateTextureMip(rec.gpu, up.mip, static_cast<int>(mip.width), static_cast<int>(mip.height), mip.rowPitch, mip.data.data(), mip.data.size())) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    used += cost;
    if (up.mip == rec.residentLevels) {
      rec.residentLevels = up.mip + 1;
    }
    uploadQueue.erase(uploadQueue.begin());
    ++done;
  }
  return done;
}
void AssetDatabase::SetGpuRHI(IRHI* rhi) {
  gpuRhi = rhi;
}
namespace {
uint16_t FloatToHalf(float f) {
  uint32_t x = 0;
  std::memcpy(&x, &f, sizeof(uint32_t));
  uint32_t sign = (x >> 16) & 0x8000u;
  int32_t exp = static_cast<int32_t>((x >> 23) & 0xffu) - 127 + 15;
  uint32_t mant = x & 0x7fffffu;
  if (exp >= 31) {
    return static_cast<uint16_t>(sign | 0x7bffu);
  }
  if (exp <= 0) {
    if (exp < -10) {
      return static_cast<uint16_t>(sign);
    }
    mant |= 0x800000u;
    uint32_t shift = static_cast<uint32_t>(1 - exp);
    uint32_t half = mant >> shift;
    uint32_t rest = mant & ((shift >= 32 ? 0xffffffffu : ((1u << shift) - 1u)));
    uint32_t halfway = 1u << (shift - 1u);
    if (rest > halfway || (rest == halfway && (half & 1u) != 0u)) {
      ++half;
    }
    return static_cast<uint16_t>(sign | (half >> 0));
  }
  uint32_t half = (static_cast<uint32_t>(exp) << 10) | (mant >> 13);
  uint32_t rest = mant & 0x1fffu;
  if (rest > 0x1000u || (rest == 0x1000u && (half & 1u) != 0u)) {
    ++half;
    if ((half & 0x7c00u) == 0x7c00u) {
      half = 0x7bffu;
    }
  }
  return static_cast<uint16_t>(sign | half);
}
void EquirectDir(int face, float u, float v, float& dx, float& dy, float& dz) {
  float nx = u * 2.0f - 1.0f;
  float ny = 1.0f - v * 2.0f;
  if (face == 0) {
    dx = 1.0f;
    dy = ny;
    dz = -nx;
  } else if (face == 1) {
    dx = -1.0f;
    dy = ny;
    dz = nx;
  } else if (face == 2) {
    dx = nx;
    dy = 1.0f;
    dz = -ny;
  } else if (face == 3) {
    dx = nx;
    dy = -1.0f;
    dz = ny;
  } else if (face == 4) {
    dx = nx;
    dy = ny;
    dz = 1.0f;
  } else {
    dx = -nx;
    dy = ny;
    dz = -1.0f;
  }
  float len = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (len > 1e-9f) {
    dx /= len;
    dy /= len;
    dz /= len;
  }
}
}
RHITexture AssetDatabase::ImportHdrSky(const std::string& path) {
  if (gpuRhi == nullptr || path.empty()) {
    return 0;
  }
  int w = 0;
  int h = 0;
  int comp = 0;
  float* hdr = stbi_loadf(path.c_str(), &w, &h, &comp, 3);
  if (hdr == nullptr || w <= 0 || h <= 0) {
    if (hdr != nullptr) {
      stbi_image_free(hdr);
    }
    return 0;
  }
  RHITexture cube = EquirectToCube(hdr, w, h);
  stbi_image_free(hdr);
  return cube;
}
RHITexture AssetDatabase::ImportHdrSkyFromMemory(const unsigned char* data, size_t size) {
  if (gpuRhi == nullptr || data == nullptr || size == 0) {
    return 0;
  }
  int w = 0;
  int h = 0;
  int comp = 0;
  float* hdr = stbi_loadf_from_memory(data, static_cast<int>(size), &w, &h, &comp, 3);
  if (hdr == nullptr || w <= 0 || h <= 0) {
    if (hdr != nullptr) {
      stbi_image_free(hdr);
    }
    return 0;
  }
  RHITexture cube = EquirectToCube(hdr, w, h);
  stbi_image_free(hdr);
  return cube;
}
RHITexture AssetDatabase::EquirectToCube(const float* hdr, int w, int h) {
  if (gpuRhi == nullptr || hdr == nullptr || w <= 0 || h <= 0) {
    return 0;
  }
  int face = w / 4;
  if (face < 16) {
    face = 16;
  }
  if (face > 512) {
    face = 512;
  }
  RHITexture cube = gpuRhi->CreateEnvCube(face);
  if (cube == 0) {
    return 0;
  }
  size_t faceBytes = static_cast<size_t>(face) * face * 4 * sizeof(uint16_t);
  size_t mark = skyScratch.SavePoint();
  uint16_t* facePx = static_cast<uint16_t*>(skyScratch.Allocate(faceBytes, alignof(uint16_t)));
  if (facePx == nullptr) {
    skyScratch.TrimTo(mark);
    return 0;
  }
  const float pi = 3.14159265359f;
  for (int f = 0; f < 6; ++f) {
    for (int y = 0; y < face; ++y) {
      for (int x = 0; x < face; ++x) {
        float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(face);
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(face);
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        EquirectDir(f, u, v, dx, dy, dz);
        float eu = std::atan2(dz, dx) / (2.0f * pi) + 0.5f;
        float ev = std::acos(dy < -1.0f ? -1.0f : (dy > 1.0f ? 1.0f : dy)) / pi;
        float sx = eu * static_cast<float>(w) - 0.5f;
        float sy = ev * static_cast<float>(h) - 0.5f;
        int x0 = static_cast<int>(std::floor(sx));
        int y0 = static_cast<int>(std::floor(sy));
        float fx = sx - static_cast<float>(x0);
        float fy = sy - static_cast<float>(y0);
        float rgb[3] = { 0.0f, 0.0f, 0.0f };
        for (int oy = 0; oy < 2; ++oy) {
          for (int ox = 0; ox < 2; ++ox) {
            int ix = x0 + ox;
            int iy = y0 + oy;
            while (ix < 0) {
              ix += w;
            }
            while (ix >= w) {
              ix -= w;
            }
            if (iy < 0) {
              iy = 0;
            }
            if (iy >= h) {
              iy = h - 1;
            }
            float wgt = (ox == 0 ? 1.0f - fx : fx) * (oy == 0 ? 1.0f - fy : fy);
            size_t si = (static_cast<size_t>(iy) * w + ix) * 3;
            rgb[0] += hdr[si + 0] * wgt;
            rgb[1] += hdr[si + 1] * wgt;
            rgb[2] += hdr[si + 2] * wgt;
          }
        }
        size_t di = (static_cast<size_t>(y) * face + x) * 4;
        facePx[di + 0] = FloatToHalf(rgb[0]);
        facePx[di + 1] = FloatToHalf(rgb[1]);
        facePx[di + 2] = FloatToHalf(rgb[2]);
        facePx[di + 3] = FloatToHalf(1.0f);
      }
    }
    uint32_t row = static_cast<uint32_t>(face) * 8;
    if (!gpuRhi->UpdateCubeFace(cube, f, face, row, facePx, static_cast<size_t>(row) * face)) {
      skyScratch.TrimTo(mark);
      return 0;
    }
  }
  gpuRhi->GenerateCubeMips(cube);
  skyScratch.TrimTo(mark);
  return cube;
}
void AssetDatabase::DropMeshGpu(MeshRecord& record) {
  if (gpuRhi != nullptr) {
    if (record.gpuVB != 0) {
      gpuRhi->DestroyBuffer(record.gpuVB);
    }
    if (record.gpuIB != 0) {
      gpuRhi->DestroyBuffer(record.gpuIB);
    }
  }
  record.gpuVB = 0;
  record.gpuIB = 0;
  record.gpuCount = 0;
  record.gpuReady = false;
  record.stageInterleaved.clear();
  record.stageVBDone = 0;
  record.stageIBDone = 0;
}
void AssetDatabase::DropTexGpu(TextureRecord& record) {
  if (gpuRhi != nullptr && record.gpu != 0) {
    gpuRhi->DestroyTexture(record.gpu);
  }
  record.gpu = 0;
  record.residentLevels = 0;
}
bool AssetDatabase::EnsureMeshGpu(const std::string& guid, size_t maxBytes) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || !record->loaded || gpuRhi == nullptr) {
    return false;
  }
  if (record->gpuReady) {
    return true;
  }
  if (maxBytes == 0) {
    return false;
  }
  size_t vertexCount = record->data.positions.size() / 3;
  if (vertexCount == 0 || record->data.indices.empty()) {
    return false;
  }
  if (!record->meshletsBuilt) {
    record->meshletsBuilt = true;
    std::vector<MeshletRange> ranges;
    if (BuildMeshlets(record->data, ranges)) {
      record->meshlets = ranges;
    }
  }
  size_t vbBytes = vertexCount * 56;
  size_t ibBytes = record->data.indices.size() * 4;
  if (record->gpuVB == 0) {
    record->stageInterleaved.clear();
    record->stageInterleaved.reserve(vertexCount * 14);
    for (size_t i = 0; i < vertexCount; ++i) {
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 0]);
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 1]);
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 2]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 0]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 1]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 2]);
      record->stageInterleaved.push_back(record->data.uvs[i * 2 + 0]);
      record->stageInterleaved.push_back(record->data.uvs[i * 2 + 1]);
      record->stageInterleaved.push_back(record->data.tangents[i * 4 + 0]);
      record->stageInterleaved.push_back(record->data.tangents[i * 4 + 1]);
      record->stageInterleaved.push_back(record->data.tangents[i * 4 + 2]);
      record->stageInterleaved.push_back(record->data.tangents[i * 4 + 3]);
      float uv2u = 0.0f;
      float uv2v = 0.0f;
      if (record->data.uv2s.size() == vertexCount * 2) {
        uv2u = record->data.uv2s[i * 2 + 0];
        uv2v = record->data.uv2s[i * 2 + 1];
      }
      record->stageInterleaved.push_back(uv2u);
      record->stageInterleaved.push_back(uv2v);
    }
    RHIBuffer vb = gpuRhi->CreateBufferEmpty(static_cast<uint64_t>(vbBytes), 56, false);
    if (vb == 0) {
      record->stageInterleaved.clear();
      return false;
    }
    RHIBuffer ib = gpuRhi->CreateBufferEmpty(static_cast<uint64_t>(ibBytes), 4, true);
    if (ib == 0) {
      gpuRhi->DestroyBuffer(vb);
      record->stageInterleaved.clear();
      return false;
    }
    record->gpuVB = vb;
    record->gpuIB = ib;
    record->stageVBDone = 0;
    record->stageIBDone = 0;
  }
  size_t budget = maxBytes;
  const unsigned char* vbytes = reinterpret_cast<const unsigned char*>(record->stageInterleaved.data());
  while (budget > 0 && record->stageVBDone < vbBytes) {
    size_t chunk = vbBytes - record->stageVBDone;
    if (chunk > budget) {
      chunk = budget;
    }
    if (!gpuRhi->UpdateBufferRange(record->gpuVB, record->stageVBDone, vbytes + record->stageVBDone, chunk)) {
      return false;
    }
    record->stageVBDone += chunk;
    budget -= chunk;
  }
  const unsigned char* ibytes = reinterpret_cast<const unsigned char*>(record->data.indices.data());
  while (budget > 0 && record->stageIBDone < ibBytes) {
    size_t chunk = ibBytes - record->stageIBDone;
    if (chunk > budget) {
      chunk = budget;
    }
    if (!gpuRhi->UpdateBufferRange(record->gpuIB, record->stageIBDone, ibytes + record->stageIBDone, chunk)) {
      return false;
    }
    record->stageIBDone += chunk;
    budget -= chunk;
  }
  if (record->stageVBDone == vbBytes && record->stageIBDone == ibBytes) {
    record->gpuCount = static_cast<uint32_t>(record->data.indices.size());
    record->gpuReady = true;
    record->stageInterleaved.clear();
    return true;
  }
  return false;
}
bool AssetDatabase::SetSourcePath(const std::string& guid, const std::string& newSourcePath) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || newSourcePath.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(newSourcePath, ec) || ec) {
    return false;
  }
  record->sourcePath = newSourcePath;
  record->data.sourcePath = newSourcePath;
  record->hasSource = true;
  record->data.hasSource = true;
  record->lastSeenSourceTime = 0;
  EncodeMeshFile(record->data, record->meshPath);
  return true;
}
bool AssetDatabase::SetTexSourcePath(const std::string& guid, const std::string& newSourcePath) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || newSourcePath.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(newSourcePath, ec) || ec) {
    return false;
  }
  it->second.sourcePath = newSourcePath;
  it->second.data.sourcePath = newSourcePath;
  it->second.hasSource = true;
  it->second.data.hasSource = true;
  it->second.lastSeenSourceTime = 0;
  EncodeTextureFile(it->second.data, it->second.texPath);
  return true;
}
}
