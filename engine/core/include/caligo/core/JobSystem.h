#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace caligo {

class JobSystem final {
public:
  JobSystem();
  ~JobSystem();

  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;

  void Initialize(uint32_t threadCount = 0);
  void Shutdown() noexcept;

  [[nodiscard]] uint32_t ThreadCount() const noexcept;

  void ParallelFor(uint32_t count, uint32_t minRange,
                   const std::function<void(uint32_t, uint32_t)>& function);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}
