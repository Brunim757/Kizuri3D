#pragma once
#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <mutex>
namespace Kizuri {
class PoolAllocator {
public:
  PoolAllocator(size_t blockSize, size_t blockCount, size_t alignment);
  ~PoolAllocator();
  bool Initialize();
  void Shutdown();
  void* Allocate();
  void Free(void* p);
  size_t BlockSize() const;
  size_t Capacity() const;
  size_t FreeCount() const;
  size_t UsedCount() const;
  bool Owns(const void* p) const;
  bool CheckHealthy() const;
private:
  size_t requestedSize;
  size_t blockSize;
  size_t blockCount;
  size_t alignment;
  unsigned char* memory;
  void* freeList;
  std::atomic<size_t> freeCount;
  size_t liveCount;
  bool initialized;
  mutable std::mutex mtx;
  unsigned char* liveBits;
  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;
};
class ArenaAllocator {
public:
  explicit ArenaAllocator(size_t capacity);
  ~ArenaAllocator();
  bool Initialize();
  void Shutdown();
  void* Allocate(size_t size, size_t alignment);
  void Reset();
  size_t SavePoint() const;
  void TrimTo(size_t point);
  size_t Used() const;
  size_t HighWater() const;
  size_t Capacity() const;
private:
  size_t capacity;
  unsigned char* memory;
  size_t offset;
  size_t highWater;
  bool initialized;
  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;
};
size_t AlignUp(size_t value, size_t alignment);
bool IsPowerOfTwo(size_t value);
}
