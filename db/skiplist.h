#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

#include <atomic>
#include <cassert>
#include <cstdlib>

#include "util/arena.h"
#include "util/random.h"





namespace leveldb {

template <typename Key, class Comparator>
class SkipList {
 private:
  struct Node; 

 public:
    
  explicit SkipList(Comparator cmp, Arena* arena);
  
  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  // 将Key插入到跳表中
  // 要求：当前list中不能存在相同的key的node
  void Insert(const Key& key); 









};












}  // namespace leveldb












#endif  //STORAGE_LEVELDB_DB_SKIPLIST_H_
