#include "Kizuri/Core.h"
#include "Kizuri/JobSystem.h"
#include <DirectXMath.h>
#include <TaskScheduler.h>
#include <zstd.h>
#include <flatbuffers/flatbuffers.h>
#include <cstring>
#include <atomic>
namespace Kizuri {
namespace {
struct FillTask : public enki::ITaskSet {
  std::atomic<int>* out;
  FillTask(std::atomic<int>* o) : out(o) { m_SetSize = 1; }
  void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
    out->store(42, std::memory_order_relaxed);
  }
};
}
const char* Core_Version() {
#ifdef KIZURI_VERSION_STR
  return KIZURI_VERSION_STR;
#else
  return "0.0.0-unknown";
#endif
}
bool Core_TestDirectXMath() {
  using namespace DirectX;
  XMVECTOR a = XMVectorSet(1.0f, 2.0f, 3.0f, 4.0f);
  XMVECTOR b = XMVectorSet(5.0f, 6.0f, 7.0f, 8.0f);
  XMVECTOR c = XMVectorAdd(a, b);
  XMFLOAT4 o;
  XMStoreFloat4(&o, c);
  return o.x == 6.0f && o.y == 8.0f && o.z == 10.0f && o.w == 12.0f;
}
bool Core_TestTaskSystem() {
  JobSystem jobs;
  if (!jobs.Initialize()) {
    return false;
  }
  if (jobs.GetThreadCount() == 0) {
    return false;
  }
  std::atomic<int> value(0);
  FillTask task(&value);
  jobs.AddTask(&task);
  jobs.WaitForTask(&task);
  bool ok = (value.load(std::memory_order_relaxed) == 42);
  ok = ok && jobs.SubmittedCount() >= 1 && jobs.CompletedCount() >= 1;
  std::atomic<int> sum(0);
  jobs.ParallelFor(256, [&](uint32_t i) {
    sum.fetch_add(static_cast<int>(i), std::memory_order_relaxed);
  });
  ok = ok && sum.load(std::memory_order_relaxed) == 32640;
  jobs.Shutdown();
  return ok;
}
bool Core_TestZstd() {
  const char* src = "KizuriEngine-Fase0-hello-zstd";
  size_t srcSize = std::strlen(src) + 1;
  size_t bound = ZSTD_compressBound(srcSize);
  if (bound == 0) {
    return false;
  }
  char comp[256];
  char decomp[256];
  if (bound > sizeof(comp)) {
    return false;
  }
  size_t csize = ZSTD_compress(comp, sizeof(comp), src, srcSize, 1);
  if (ZSTD_isError(csize)) {
    return false;
  }
  size_t dsize = ZSTD_decompress(decomp, sizeof(decomp), comp, csize);
  if (ZSTD_isError(dsize)) {
    return false;
  }
  if (dsize != srcSize) {
    return false;
  }
  return std::memcmp(src, decomp, srcSize) == 0;
}
bool Core_TestFlatBuffers() {
  flatbuffers::FlatBufferBuilder builder(64);
  auto s = builder.CreateString("KizuriEngine-Fase0");
  builder.Finish(s);
  uint8_t* buf = builder.GetBufferPointer();
  flatbuffers::uoffset_t sz = builder.GetSize();
  return buf != nullptr && sz > 0;
}
}
