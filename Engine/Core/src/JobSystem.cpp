#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/JobSystem.h"
#include "Kizuri/Profile.h"
#include <TaskScheduler.h>
#include <windows.h>
#include <malloc.h>
#include <atomic>
#include <cstdio>
namespace Kizuri {
namespace {
std::atomic<uint64_t> g_submitted(0);
std::atomic<uint64_t> g_completed(0);
std::atomic<uint64_t> g_taskBytes(0);
std::atomic<uint64_t> g_taskHighWater(0);
void* TaskAlloc(size_t align_, size_t size_, void* userData_, const char* file_, int line_) {
  (void)userData_;
  (void)file_;
  (void)line_;
  void* p = _aligned_malloc(size_ == 0 ? 1 : size_, align_ == 0 ? 8 : align_);
  if (p != nullptr) {
    uint64_t now = g_taskBytes.fetch_add(static_cast<uint64_t>(size_), std::memory_order_relaxed) + static_cast<uint64_t>(size_);
    uint64_t hw = g_taskHighWater.load(std::memory_order_relaxed);
    while (now > hw && !g_taskHighWater.compare_exchange_weak(hw, now, std::memory_order_relaxed)) {
    }
  }
  return p;
}
void TaskFree(void* ptr_, size_t size_, void* userData_, const char* file_, int line_) {
  (void)userData_;
  (void)file_;
  (void)line_;
  if (ptr_ == nullptr) {
    return;
  }
  g_taskBytes.fetch_sub(static_cast<uint64_t>(size_), std::memory_order_relaxed);
  _aligned_free(ptr_);
}
void JobThreadStart(uint32_t threadNum_) {
  wchar_t name[32];
  _snwprintf_s(name, 32, _TRUNCATE, L"KizuriJob%u", threadNum_);
  SetThreadDescription(GetCurrentThread(), name);
  Profiler::NoteThreadStart(threadNum_);
}
void JobThreadStop(uint32_t threadNum_) {
  Profiler::NoteThreadStop(threadNum_);
}
enki::TaskSchedulerConfig MakeConfig(uint32_t totalThreads_) {
  enki::TaskSchedulerConfig cfg;
  if (totalThreads_ > 0) {
    cfg.numTaskThreadsToCreate = totalThreads_ > 0 ? totalThreads_ - 1 : 0;
  }
  cfg.profilerCallbacks.threadStart = &JobThreadStart;
  cfg.profilerCallbacks.threadStop = &JobThreadStop;
  cfg.customAllocator.alloc = &TaskAlloc;
  cfg.customAllocator.free = &TaskFree;
  cfg.customAllocator.userData = nullptr;
  return cfg;
}
struct ForTask : public enki::ITaskSet {
  const std::function<void(uint32_t)>* fn;
  ForTask(uint32_t count_, const std::function<void(uint32_t)>* fn_, uint32_t minRange_) {
    m_SetSize = count_;
    m_MinRange = minRange_ == 0 ? 1 : minRange_;
    fn = fn_;
  }
  void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override {
    (void)threadnum_;
    for (uint32_t i = range_.start; i < range_.end; ++i) {
      (*fn)(i);
    }
  }
};
enki::TaskPriority ToEnkiPriority(TaskPriority p) {
  switch (p) {
    case TaskPriority::High:
      return enki::TASK_PRIORITY_HIGH;
    case TaskPriority::Low:
#if ENKITS_TASK_PRIORITIES_NUM > 1
      return enki::TASK_PRIORITY_LOW;
#else
      return enki::TASK_PRIORITY_HIGH;
#endif
    case TaskPriority::Medium:
    default:
#if ENKITS_TASK_PRIORITIES_NUM > 2
      return enki::TASK_PRIORITY_MED;
#else
      return enki::TASK_PRIORITY_HIGH;
#endif
  }
}
}
JobSystem::JobSystem()
  : scheduler(nullptr)
  , initialized(false) {
}
JobSystem::~JobSystem() {
  Shutdown();
}
bool JobSystem::Initialize() {
  if (initialized) {
    return true;
  }
  scheduler = new enki::TaskScheduler();
  scheduler->Initialize(MakeConfig(0));
  if (scheduler->GetNumTaskThreads() == 0) {
    delete scheduler;
    scheduler = nullptr;
    return false;
  }
  initialized = true;
  return true;
}
bool JobSystem::Initialize(uint32_t totalThreads) {
  if (initialized) {
    return true;
  }
  if (totalThreads == 0) {
    return false;
  }
  scheduler = new enki::TaskScheduler();
  scheduler->Initialize(MakeConfig(totalThreads));
  initialized = true;
  return true;
}
void JobSystem::Shutdown() {
  if (!initialized) {
    if (scheduler != nullptr) {
      delete scheduler;
      scheduler = nullptr;
    }
    return;
  }
  scheduler->WaitforAllAndShutdown();
  delete scheduler;
  scheduler = nullptr;
  initialized = false;
}
uint32_t JobSystem::GetThreadCount() const {
  if (!initialized || scheduler == nullptr) {
    return 0;
  }
  return scheduler->GetNumTaskThreads();
}
uint32_t JobSystem::GetThreadNum() const {
  if (!initialized || scheduler == nullptr) {
    return 0;
  }
  return scheduler->GetThreadNum();
}
void JobSystem::AddTask(enki::ITaskSet* task) {
  AddTask(task, TaskPriority::High);
}
void JobSystem::AddTask(enki::ITaskSet* task, TaskPriority priority) {
  if (initialized && scheduler != nullptr && task != nullptr) {
    task->m_Priority = ToEnkiPriority(priority);
    scheduler->AddTaskSetToPipe(task);
    g_submitted.fetch_add(1, std::memory_order_relaxed);
  }
}
void JobSystem::ParallelFor(uint32_t count, const std::function<void(uint32_t index)>& fn, uint32_t minRange) {
  if (!initialized || scheduler == nullptr || count == 0) {
    for (uint32_t i = 0; i < count; ++i) {
      fn(i);
    }
    return;
  }
  ForTask task(count, &fn, minRange);
  task.m_Priority = enki::TASK_PRIORITY_HIGH;
  scheduler->AddTaskSetToPipe(&task);
  g_submitted.fetch_add(1, std::memory_order_relaxed);
  scheduler->WaitforTask(&task);
  g_completed.fetch_add(1, std::memory_order_relaxed);
}
void JobSystem::WaitForTask(enki::ITaskSet* task) {
  if (initialized && scheduler != nullptr && task != nullptr) {
    scheduler->WaitforTask(task);
    g_completed.fetch_add(1, std::memory_order_relaxed);
  }
}
bool JobSystem::IsInitialized() const {
  return initialized;
}
uint64_t JobSystem::SubmittedCount() const {
  return g_submitted.load(std::memory_order_relaxed);
}
uint64_t JobSystem::CompletedCount() const {
  return g_completed.load(std::memory_order_relaxed);
}
uint64_t JobSystem::TaskAllocBytes() const {
  return g_taskBytes.load(std::memory_order_relaxed);
}
uint64_t JobSystem::TaskAllocHighWater() const {
  return g_taskHighWater.load(std::memory_order_relaxed);
}
}
