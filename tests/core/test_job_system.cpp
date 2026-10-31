#include <algorithm>
#include <caligo/core/JobSystem.h>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

TEST_CASE("job system visits every index exactly once") {
  caligo::JobSystem jobs;
  jobs.Initialize(2);
  CHECK(jobs.ThreadCount() >= 2);

  std::vector<int> visits(1000, 0);
  jobs.ParallelFor(1000, 64, [&visits](uint32_t begin, uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      ++visits[static_cast<size_t>(index)];
    }
  });

  CHECK(std::all_of(visits.begin(), visits.end(), [](int value) { return value == 1; }));

  jobs.ParallelFor(1000, 0, [&visits](uint32_t begin, uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      ++visits[static_cast<size_t>(index)];
    }
  });

  CHECK(std::all_of(visits.begin(), visits.end(), [](int value) { return value == 2; }));

  bool called = false;
  jobs.ParallelFor(0, 1, [&called](uint32_t, uint32_t) { called = true; });
  CHECK_FALSE(called);

  jobs.Shutdown();
  CHECK(jobs.ThreadCount() == 0);
  jobs.Shutdown();
  CHECK(jobs.ThreadCount() == 0);
}
