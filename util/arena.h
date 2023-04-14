#ifndef STORAGE_LEVELDB_UNIL_ARENA_H
#define STORAGE_LEVELDB_UNIL_ARENA_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace leveldb {

class Arena {
 public:
  Arena();

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  ~Arena();

  char* Allocate(size_t bytes);

  char* AllocateAligned(size_t bytes);

  size_t  MemoryUsage() const {
    return memory_usage_.load(std::memory_order_relaxed);
  }
  private:
  char* AllocateFallback(size_t bytes);
  char* AllocateNewBlock(size_t block_bytes);

  // 内存分配状态
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_; 

  std::vector<char*> blocks; // 已分配的内存块的数组

  std::atomic<size_t>memory_usage_; // 已使用内存的大小
};

inline char* Arena::Allocate(size_t bytes) {
  assert(bytes >= 0);
  if(bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}



}   // namespace leveldb






#endif  // STORAGE_LEVELDB_UNIL_ARENA_H