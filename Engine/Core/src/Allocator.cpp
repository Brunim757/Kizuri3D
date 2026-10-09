#include "Kizuri/Allocator.h"
#include <malloc.h>
#include <cstring>
#include <new>
namespace Kizuri {
size_t AlignUp(size_t value, size_t alignment) {
  if (alignment == 0) {
    return value;
  }
  if (!IsPowerOfTwo(alignment)) {
    return value;
  }
  size_t mask = alignment - 1;
  return (value + mask) & ~mask;
}
bool IsPowerOfTwo(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}
PoolAllocator::PoolAllocator(size_t bSize, size_t bCount, size_t align)
  : requestedSize(bSize)
  , blockSize(0)
  , blockCount(bCount)
  , alignment(align == 0 ? 8 : align)
  , memory(nullptr)
  , freeList(nullptr)
  , freeCount(0)
  , liveCount(0)
  , initialized(false)
  , liveBits(nullptr) {
  if (!IsPowerOfTwo(alignment)) {
    alignment = 8;
  }
  size_t minSize = sizeof(void*);
  size_t base = requestedSize < minSize ? minSize : requestedSize;
  blockSize = AlignUp(base, alignment);
}
PoolAllocator::~PoolAllocator() {
  Shutdown();
}
bool PoolAllocator::Initialize() {
  if (initialized) {
    return true;
  }
  if (blockCount == 0 || blockSize == 0) {
    return false;
  }
  size_t total = blockSize * blockCount;
  memory = static_cast<unsigned char*>(_aligned_malloc(total, alignment));
  if (memory == nullptr) {
    return false;
  }
  size_t words = (blockCount + 7) / 8;
  liveBits = new (std::nothrow) unsigned char[words];
  if (liveBits == nullptr) {
    _aligned_free(memory);
    memory = nullptr;
    return false;
  }
  std::memset(liveBits, 0, words);
  freeList = nullptr;
  for (size_t i = 0; i < blockCount; ++i) {
    void* block = memory + (i * blockSize);
    *static_cast<void**>(block) = freeList;
    freeList = block;
  }
  freeCount.store(blockCount, std::memory_order_relaxed);
  liveCount = 0;
  initialized = true;
  return true;
}
void PoolAllocator::Shutdown() {
  if (!initialized) {
    return;
  }
  _aligned_free(memory);
  memory = nullptr;
  delete[] liveBits;
  liveBits = nullptr;
  freeList = nullptr;
  freeCount.store(0, std::memory_order_relaxed);
  liveCount = 0;
  initialized = false;
}
void* PoolAllocator::Allocate() {
  std::lock_guard<std::mutex> lock(mtx);
  if (freeList == nullptr) {
    return nullptr;
  }
  void* block = freeList;
  freeList = *static_cast<void**>(block);
  size_t idx = (static_cast<unsigned char*>(block) - memory) / blockSize;
  if (idx < blockCount) {
    liveBits[idx / 8] |= static_cast<unsigned char>(1u << (idx % 8));
    ++liveCount;
  }
  freeCount.fetch_sub(1, std::memory_order_relaxed);
  return block;
}
void PoolAllocator::Free(void* p) {
  if (p == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);
  if (!Owns(p)) {
    return;
  }
  size_t idx = (static_cast<unsigned char*>(p) - memory) / blockSize;
  if (idx >= blockCount) {
    return;
  }
  unsigned char bit = static_cast<unsigned char>(1u << (idx % 8));
  if ((liveBits[idx / 8] & bit) == 0) {
    return;
  }
  liveBits[idx / 8] &= static_cast<unsigned char>(~bit);
  if (liveCount > 0) {
    --liveCount;
  }
  *static_cast<void**>(p) = freeList;
  freeList = p;
  size_t fc = freeCount.load(std::memory_order_relaxed);
  if (fc < blockCount) {
    freeCount.fetch_add(1, std::memory_order_relaxed);
  }
}
size_t PoolAllocator::BlockSize() const {
  return blockSize;
}
size_t PoolAllocator::Capacity() const {
  return blockCount;
}
size_t PoolAllocator::FreeCount() const {
  return freeCount.load(std::memory_order_relaxed);
}
size_t PoolAllocator::UsedCount() const {
  std::lock_guard<std::mutex> lock(mtx);
  return liveCount;
}
bool PoolAllocator::Owns(const void* p) const {
  if (memory == nullptr || p == nullptr) {
    return false;
  }
  const unsigned char* c = static_cast<const unsigned char*>(p);
  if (c < memory || c >= memory + blockSize * blockCount) {
    return false;
  }
  return ((c - memory) % blockSize) == 0;
}
bool PoolAllocator::CheckHealthy() const {
  std::lock_guard<std::mutex> lock(mtx);
  size_t counted = 0;
  void* cur = freeList;
  while (cur != nullptr) {
    const unsigned char* c = static_cast<const unsigned char*>(cur);
    if (c < memory || c >= memory + blockSize * blockCount || ((c - memory) % blockSize) != 0) {
      return false;
    }
    cur = *static_cast<void* const*>(cur);
    ++counted;
    if (counted > blockCount) {
      return false;
    }
  }
  return counted + liveCount == blockCount;
}
ArenaAllocator::ArenaAllocator(size_t cap)
  : capacity(cap)
  , memory(nullptr)
  , offset(0)
  , highWater(0)
  , initialized(false) {
}
ArenaAllocator::~ArenaAllocator() {
  Shutdown();
}
bool ArenaAllocator::Initialize() {
  if (initialized) {
    return true;
  }
  if (capacity == 0) {
    return false;
  }
  memory = static_cast<unsigned char*>(_aligned_malloc(capacity, 16));
  if (memory == nullptr) {
    return false;
  }
  offset = 0;
  highWater = 0;
  initialized = true;
  return true;
}
void ArenaAllocator::Shutdown() {
  if (!initialized) {
    return;
  }
  _aligned_free(memory);
  memory = nullptr;
  offset = 0;
  highWater = 0;
  initialized = false;
}
void* ArenaAllocator::Allocate(size_t size, size_t alignment) {
  if (!initialized || size == 0) {
    return nullptr;
  }
  size_t align = alignment == 0 ? 8 : alignment;
  if (!IsPowerOfTwo(align)) {
    align = 8;
  }
  size_t aligned = AlignUp(offset, align);
  if (aligned + size > capacity) {
    return nullptr;
  }
  void* p = memory + aligned;
  offset = aligned + size;
  if (offset > highWater) {
    highWater = offset;
  }
  return p;
}
void ArenaAllocator::Reset() {
  offset = 0;
}
size_t ArenaAllocator::SavePoint() const {
  return offset;
}
void ArenaAllocator::TrimTo(size_t point) {
  if (point <= offset) {
    offset = point;
  }
}
size_t ArenaAllocator::Used() const {
  return offset;
}
size_t ArenaAllocator::HighWater() const {
  return highWater;
}
size_t ArenaAllocator::Capacity() const {
  return capacity;
}
}
