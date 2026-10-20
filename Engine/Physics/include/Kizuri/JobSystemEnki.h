#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Color.h>
#include <memory>
#include <mutex>
#include <vector>
namespace Kizuri {
class JobSystem;
class JobSystemEnki : public JPH::JobSystemWithBarrier {
public:
  explicit JobSystemEnki(Kizuri::JobSystem* jobs);
  ~JobSystemEnki() override;
  int GetMaxConcurrency() const override;
  JPH::JobSystem::JobHandle CreateJob(const char* inName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0) override;
  void FreeJob(JPH::JobSystem::Job* inJob) override;
  void QueueJob(JPH::JobSystem::Job* inJob) override;
  void QueueJobs(JPH::JobSystem::Job** inJobs, JPH::uint inNumJobs) override;
private:
  struct TaskSlot;
  struct Pool;
  TaskSlot* AcquireSlot();
  JobSystem* engineJobs;
  std::shared_ptr<Pool> pool;
};
}
