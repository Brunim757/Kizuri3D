#pragma once
#include <cstdint>
#include <string>
namespace Kizuri {
struct HardwareSpec {
  uint64_t videoMemoryBytes = 0;
  uint64_t systemMemoryBytes = 0;
  uint32_t coreCount = 0;
  std::string gpuName;
  bool lowSpec = false;
  std::string Summary() const;
};
HardwareSpec DetectHardware();
}