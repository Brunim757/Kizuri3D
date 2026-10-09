#include "Kizuri/Profile.h"
namespace Kizuri {
namespace Profiler {
namespace {
ZoneBeginFn g_begin = nullptr;
ZoneEndFn g_end = nullptr;
ThreadNoteFn g_threadStart = nullptr;
ThreadNoteFn g_threadStop = nullptr;
}
void SetZoneHooks(ZoneBeginFn beginFn, ZoneEndFn endFn) {
  g_begin = beginFn;
  g_end = endFn;
}
void SetThreadHooks(ThreadNoteFn startFn, ThreadNoteFn stopFn) {
  g_threadStart = startFn;
  g_threadStop = stopFn;
}
void BeginZone(const char* name, const char* file, int line) {
  if (g_begin != nullptr) {
    g_begin(name, file, line);
  }
}
void EndZone() {
  if (g_end != nullptr) {
    g_end();
  }
}
void NoteThreadStart(uint32_t threadNum) {
  if (g_threadStart != nullptr) {
    g_threadStart(threadNum);
  }
}
void NoteThreadStop(uint32_t threadNum) {
  if (g_threadStop != nullptr) {
    g_threadStop(threadNum);
  }
}
Zone::Zone(const char* name, const char* file, int line) {
  BeginZone(name, file, line);
}
Zone::~Zone() {
  EndZone();
}
}
}
