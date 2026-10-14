#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Color.h>
#include <atomic>
#include <mutex>
#include <vector>
namespace Kizuri {
class JobSystem;
class JobSystemEnki : public JPH::JobSystemWithBarrier {
public:
  explicit JobSystemEnki(Kizuri::JobSystem* jobs);
  int GetMaxConcurrency() const override;
  JPH::JobSystem::JobHandle CreateJob(const char* inName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0) override;
  void FreeJob(JPH::JobSystem::Job* inJob) override;
  void QueueJob(JPH::JobSystem::Job* inJob) override;
  void QueueJobs(JPH::JobSystem::Job** inJobs, JPH::uint inNumJobs) override;
private:
  struct TaskSlot;
  TaskSlot* AcquireSlot();
  void ReleaseSlot(TaskSlot* slot);
  Kizuri::JobSystem* engineJobs;
  std::mutex poolMutex;
  std::vector<TaskSlot*> freeSlots;
  std::vector<TaskSlot*> ownedSlots;
  std::atomic<int> inFlight;
};
}
