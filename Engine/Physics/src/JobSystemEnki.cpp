#include "Kizuri/JobSystemEnki.h"
#include "Kizuri/JobSystem.h"
#include <TaskScheduler.h>
#include <atomic>
#include <chrono>
#include <thread>
namespace Kizuri {
struct JobSystemEnki::Pool {
  std::mutex mutex;
  std::vector<TaskSlot*> slots;
  std::vector<TaskSlot*> freeSlots;
  std::atomic<int> inFlight{ 0 };
  bool alive = true;
};
struct JobSystemEnki::TaskSlot : public enki::ITaskSet {
  JPH::JobSystem::Job* joltJob = nullptr;
  std::weak_ptr<JobSystemEnki::Pool> pool;
  TaskSlot() {
    m_SetSize = 1;
    m_MinRange = 1;
  }
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override {
    (void)range;
    (void)threadnum;
    JPH::JobSystem::Job* job = joltJob;
    joltJob = nullptr;
    if (job != nullptr) {
      job->Execute();
      job->Release();
    }
    std::shared_ptr<JobSystemEnki::Pool> p = pool.lock();
    if (p) {
      {
        std::lock_guard<std::mutex> lock(p->mutex);
        if (p->alive) {
          p->freeSlots.push_back(this);
        }
      }
      p->inFlight.fetch_sub(1, std::memory_order_release);
    }
  }
};
JobSystemEnki::JobSystemEnki(Kizuri::JobSystem* jobs)
  : engineJobs(jobs)
  , pool(std::make_shared<Pool>()) {
}
JobSystemEnki::~JobSystemEnki() {
  Pool* p = pool.get();
  if (p == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(p->mutex);
    p->alive = false;
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (p->inFlight.load(std::memory_order_acquire) > 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  bool drained = false;
  while (std::chrono::steady_clock::now() < deadline) {
    bool allComplete = true;
    {
      std::lock_guard<std::mutex> lock(p->mutex);
      for (size_t i = 0; i < p->slots.size(); ++i) {
        if (!p->slots[i]->GetIsComplete()) {
          allComplete = false;
          break;
        }
      }
    }
    if (allComplete) {
      drained = true;
      break;
    }
    std::this_thread::yield();
  }
  {
    std::lock_guard<std::mutex> lock(p->mutex);
    for (size_t i = 0; i < p->slots.size(); ++i) {
      if (drained) {
        delete p->slots[i];
      }
    }
    p->slots.clear();
    p->freeSlots.clear();
  }
  pool.reset();
}
int JobSystemEnki::GetMaxConcurrency() const {
  if (engineJobs != nullptr) {
    return static_cast<int>(engineJobs->GetThreadCount());
  }
  return 1;
}
JobSystemEnki::TaskSlot* JobSystemEnki::AcquireSlot() {
  std::lock_guard<std::mutex> lock(pool->mutex);
  for (size_t i = 0; i < pool->freeSlots.size(); ++i) {
    if (pool->freeSlots[i]->GetIsComplete()) {
      TaskSlot* slot = pool->freeSlots[i];
      pool->freeSlots.erase(pool->freeSlots.begin() + i);
      return slot;
    }
  }
  if (pool->slots.size() >= 512) {
    return nullptr;
  }
  TaskSlot* slot = new TaskSlot();
  slot->pool = pool;
  pool->slots.push_back(slot);
  return slot;
}
JPH::JobSystem::JobHandle JobSystemEnki::CreateJob(const char* inName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction& inJobFunction, JPH::uint32 inNumDependencies) {
  JPH::JobSystem::Job* job = new JPH::JobSystem::Job(inName, inColor, this, inJobFunction, inNumDependencies);
  return JPH::JobSystem::JobHandle(job);
}
void JobSystemEnki::FreeJob(JPH::JobSystem::Job* inJob) {
  delete inJob;
}
void JobSystemEnki::QueueJob(JPH::JobSystem::Job* inJob) {
  if (inJob == nullptr) {
    return;
  }
  inJob->AddRef();
  TaskSlot* slot = AcquireSlot();
  if (slot == nullptr || engineJobs == nullptr) {
    inJob->Execute();
    inJob->Release();
    if (slot != nullptr) {
      std::lock_guard<std::mutex> lock(pool->mutex);
      pool->freeSlots.push_back(slot);
    }
    return;
  }
  slot->joltJob = inJob;
  pool->inFlight.fetch_add(1, std::memory_order_relaxed);
  engineJobs->AddTask(slot);
}
void JobSystemEnki::QueueJobs(JPH::JobSystem::Job** inJobs, JPH::uint inNumJobs) {
  for (JPH::uint i = 0; i < inNumJobs; ++i) {
    QueueJob(inJobs[i]);
  }
}
}
