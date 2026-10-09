#pragma once
#include <stdint.h>
#include <functional>
namespace enki {
class TaskScheduler;
struct ITaskSet;
}
namespace Kizuri {
enum class TaskPriority {
  High = 0,
  Medium = 1,
  Low = 2
};
class JobSystem {
public:
  JobSystem();
  ~JobSystem();
  bool Initialize();
  bool Initialize(uint32_t totalThreads);
  void Shutdown();
  uint32_t GetThreadCount() const;
  uint32_t GetThreadNum() const;
  void AddTask(enki::ITaskSet* task);
  void AddTask(enki::ITaskSet* task, TaskPriority priority);
  void ParallelFor(uint32_t count, const std::function<void(uint32_t index)>& fn, uint32_t minRange = 64);
  void WaitForTask(enki::ITaskSet* task);
  bool IsInitialized() const;
  uint64_t SubmittedCount() const;
  uint64_t CompletedCount() const;
  uint64_t TaskAllocBytes() const;
  uint64_t TaskAllocHighWater() const;
private:
  enki::TaskScheduler* scheduler;
  bool initialized;
  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
};
}
