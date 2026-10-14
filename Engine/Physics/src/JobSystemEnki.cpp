#include "Kizuri/JobSystemEnki.h"
#include "Kizuri/JobSystem.h"
#include <TaskScheduler.h>
namespace Kizuri {
struct JobSystemEnki::TaskSlot : public enki::ITaskSet {
  JPH::JobSystem::Job* joltJob = nullptr;
  JobSystemEnki* owner = nullptr;
  TaskSlot() {
    m_SetSize = 1;
    m_MinRange = 1;
  }
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override {
    (void)range;
    (void)threadnum;
    if (joltJob != nullptr) {
      joltJob->Execute();
      joltJob->Release();
      joltJob = nullptr;
    }
    if (owner != nullptr) {
      owner->ReleaseSlot(this);
    }
  }
};
JobSystemEnki::JobSystemEnki(Kizuri::JobSystem* jobs)
  : engineJobs(jobs)
  , inFlight(0) {
}
int JobSystemEnki::GetMaxConcurrency() const {
  if (engineJobs != nullptr) {
    return static_cast<int>(engineJobs->GetThreadCount());
  }
  return 1;
}
JobSystemEnki::TaskSlot* JobSystemEnki::AcquireSlot() {
  std::lock_guard<std::mutex> lock(poolMutex);
  if (!freeSlots.empty()) {
    TaskSlot* slot = freeSlots.back();
    freeSlots.pop_back();
    return slot;
  }
  if (ownedSlots.size() >= 512) {
    return nullptr;
  }
  TaskSlot* slot = new TaskSlot();
  slot->owner = this;
  ownedSlots.push_back(slot);
  return slot;
}
void JobSystemEnki::ReleaseSlot(TaskSlot* slot) {
  if (slot == nullptr) {
    return;
  }
  inFlight.fetch_sub(1, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(poolMutex);
  freeSlots.push_back(slot);
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
      ReleaseSlot(slot);
    }
    return;
  }
  slot->joltJob = inJob;
  inFlight.fetch_add(1, std::memory_order_relaxed);
  engineJobs->AddTask(slot);
}
void JobSystemEnki::QueueJobs(JPH::JobSystem::Job** inJobs, JPH::uint inNumJobs) {
  for (JPH::uint i = 0; i < inNumJobs; ++i) {
    QueueJob(inJobs[i]);
  }
}
}
