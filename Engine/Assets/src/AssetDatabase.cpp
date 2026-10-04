#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include <cctype>
#include <cmath>
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
      if (ImportGltfMesh(sourcePath, keepGuid, result.data, nullptr)) {
        progress = 60;
        if (EncodeMeshFile(result.data, meshPath)) {
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
AssetDatabase::AssetDatabase()
  : jobsReady(false)
  , nextTaskId(1) {
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
}
void AssetDatabase::EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  std::unique_ptr<ImportMeshTask> task(new ImportMeshTask());
  task->sourcePath = sourcePath;
  task->meshPath = meshPath;
  task->keepGuid = keepGuid;
  task->taskId = nextTaskId++;
  task->outMutex = &completedMutex;
  task->outQueue = &completed;
  jobs.AddTask(task.get());
  pending.push_back(std::move(task));
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
    std::string ext = it->path().extension().string();
    for (size_t i = 0; i < ext.size(); ++i) {
      ext[i] = static_cast<char>(tolower(ext[i]));
    }
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
    std::unique_ptr<ImportTexTask> task(new ImportTexTask());
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
    std::unique_ptr<ImportMeshTask> task(new ImportMeshTask());
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
  std::unique_ptr<HashTask> task(new HashTask());
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
  std::unique_ptr<ImportTexTask> task(new ImportTexTask());
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
    }
    for (size_t p = 0; p < pending.size(); ++p) {
      if (pending[p]->taskId == results[i].taskId) {
        pending.erase(pending.begin() + p);
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
  return pending.size() + pendingTex.size();
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
    std::string ext = it->path().extension().string();
    for (size_t i = 0; i < ext.size(); ++i) {
      ext[i] = static_cast<char>(tolower(ext[i]));
    }
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
    for (size_t m = 0; m < mr->second.data.materials.size(); ++m) {
      const MeshMaterialData& mm = mr->second.data.materials[m];
      const std::string* slots[3] = { &mm.albedoTexGuid, &mm.normalTexGuid, &mm.ormTexGuid };
      for (int s = 0; s < 3; ++s) {
        RequestTexture(*slots[s], dist);
      }
    }
  }
}
void AssetDatabase::RequestTexture(const std::string& guid, float dist) {
  if (guid.empty()) {
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
  size_t vbBytes = vertexCount * 48;
  size_t ibBytes = record->data.indices.size() * 4;
  if (record->gpuVB == 0) {
    record->stageInterleaved.clear();
    record->stageInterleaved.reserve(vertexCount * 12);
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
    }
    RHIBuffer vb = gpuRhi->CreateBufferEmpty(static_cast<uint64_t>(vbBytes), 48, false);
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
