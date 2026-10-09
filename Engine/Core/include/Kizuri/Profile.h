#pragma once
#include <stdint.h>
namespace Kizuri {
namespace Profiler {
using ZoneBeginFn = void (*)(const char* name, const char* file, int line);
using ZoneEndFn = void (*)();
using ThreadNoteFn = void (*)(uint32_t threadNum);
void SetZoneHooks(ZoneBeginFn beginFn, ZoneEndFn endFn);
void SetThreadHooks(ThreadNoteFn startFn, ThreadNoteFn stopFn);
void BeginZone(const char* name, const char* file, int line);
void EndZone();
void NoteThreadStart(uint32_t threadNum);
void NoteThreadStop(uint32_t threadNum);
class Zone {
public:
  Zone(const char* name, const char* file, int line);
  ~Zone();
private:
  Zone(const Zone&) = delete;
  Zone& operator=(const Zone&) = delete;
};
}
}
#define KZ_PROFILE_SCOPE(name) Kizuri::Profiler::Zone kzProfileZone##__LINE__(name, __FILE__, __LINE__)
#define KZ_PROFILE_BEGIN(name) Kizuri::Profiler::BeginZone(name, __FILE__, __LINE__)
#define KZ_PROFILE_END() Kizuri::Profiler::EndZone()
