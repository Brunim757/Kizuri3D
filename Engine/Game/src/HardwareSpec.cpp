#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/HardwareSpec.h"
#include <windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <thread>
#include <cstdio>
namespace Kizuri {
using Microsoft::WRL::ComPtr;
std::string HardwareSpec::Summary() const {
  char buf[512];
  std::snprintf(buf, sizeof(buf), "GPU: %s | VRAM: %llu MB | RAM: %llu MB | Nucleos: %u | perfil: %s", gpuName.empty() ? "desconhecida" : gpuName.c_str(), static_cast<unsigned long long>(videoMemoryBytes / (1024 * 1024)), static_cast<unsigned long long>(systemMemoryBytes / (1024 * 1024)), coreCount, lowSpec ? "LOW SPEC" : "padrao");
  return std::string(buf);
}
HardwareSpec DetectHardware() {
  HardwareSpec spec;
  spec.coreCount = std::thread::hardware_concurrency();
  MEMORYSTATUSEX ms;
  ms.dwLength = sizeof(ms);
  if (GlobalMemoryStatusEx(&ms)) {
    spec.systemMemoryBytes = static_cast<uint64_t>(ms.ullTotalPhys);
  }
  ComPtr<IDXGIFactory1> factory;
  if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf())))) {
    ComPtr<IDXGIAdapter1> best;
    uint64_t bestVram = 0;
    for (UINT i = 0;; ++i) {
      ComPtr<IDXGIAdapter1> adapter;
      if (factory->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (adapter == nullptr) {
        continue;
      }
      DXGI_ADAPTER_DESC1 desc;
      if (FAILED(adapter->GetDesc1(&desc))) {
        continue;
      }
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }
      if (desc.DedicatedVideoMemory > bestVram) {
        bestVram = desc.DedicatedVideoMemory;
        best = adapter;
        wchar_t name[128];
        if (WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr) > 1) {
          std::string narrow(256, '\0');
          if (WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, narrow.data(), static_cast<int>(narrow.size()), nullptr, nullptr) > 1) {
            spec.gpuName = narrow.c_str();
          }
        }
      }
    }
    spec.videoMemoryBytes = bestVram;
  }
  const bool weakGpu = spec.videoMemoryBytes < 2ull * 1024 * 1024 * 1024;
  const bool weakRam = spec.systemMemoryBytes < 6ull * 1024 * 1024 * 1024;
  const bool weakCpu = spec.coreCount < 4;
  spec.lowSpec = weakGpu || weakRam || weakCpu;
  return spec;
}
}