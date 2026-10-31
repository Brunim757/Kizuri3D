#include <caligo/core/Assert.h>
#include <caligo/core/JobSystem.h>
#include <caligo/core/Log.h>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <TaskScheduler.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <memory>

namespace caligo {

struct JobSystem::Impl {
  enki::TaskScheduler scheduler;
  bool initialized = false;
};

JobSystem::JobSystem() : m_impl(std::make_unique<Impl>()) {}

JobSystem::~JobSystem() {
  Shutdown();
}

void JobSystem::Initialize(uint32_t threadCount) {
  if (m_impl->initialized) {
    Shutdown();
  }

  if (threadCount == 0) {
    m_impl->scheduler.Initialize();
  } else {
    m_impl->scheduler.Initialize(threadCount);
  }

  m_impl->initialized = true;
  CALIGO_LOG_INFO("Job system initialized with {} threads", ThreadCount());
}

void JobSystem::Shutdown() noexcept {
  if (!m_impl->initialized) {
    return;
  }
  m_impl->scheduler.WaitforAllAndShutdown();
  m_impl->initialized = false;
}

uint32_t JobSystem::ThreadCount() const noexcept {
  if (!m_impl->initialized) {
    return 0;
  }
  return m_impl->scheduler.GetNumTaskThreads();
}

void JobSystem::ParallelFor(uint32_t count, uint32_t minRange,
                            const std::function<void(uint32_t, uint32_t)>& function) {
  CALIGO_ASSERT_MSG(static_cast<bool>(function), "parallel function is empty");
  if (!function || count == 0) {
    return;
  }

  if (!m_impl->initialized) {
    for (uint32_t index = 0; index < count; ++index) {
      function(index, index + 1);
    }
    return;
  }

  enki::TaskSet task(count, [&function](enki::TaskSetPartition partition, uint32_t) {
    function(partition.start, partition.end);
  });
  task.m_MinRange = minRange == 0 ? 1 : minRange;

  m_impl->scheduler.AddTaskSetToPipe(&task);
  m_impl->scheduler.WaitforTask(&task);
}

}
