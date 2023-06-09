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

  // 将Key插入到list中
  // 要求：当前list中不能存在相同的key的node
  void Insert(const Key& key); 

  // 如果list包含key返回true
  bool Contains(const Key& key) const;

  // 遍历调表的迭代器
  class Iterator {
   public:
    explicit Iterator(const SkipList* list);

    // 如果迭代器位于valid node 返回 true
    bool Valid() const;

    const Key& key() const;

    void Next();

    void Prev();

    // 找到第一个 >= target 的 entry
    void Seek(const Key& target);

    void SeekToFirst();

    void SeekToLast();

   private:
    const SkipList* list_;
    Node* node_;
    // Intentionally copyable
  };
 private:
  enum { kMaxHeight = 12 };

  inline int GetMaxHight() const {
    return max_height_.load(std::memory_order_relaxed);
  }

  Node* NewNode(const Key& key, int height);
  int RandomHeight();
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

  // 这个key是否大于Node n's key
  bool KeyIsAfterNode(const Key& key, Node* n) const;

  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

  Node* FindLessThan(const Key& key) const;

  Node* FindLast() const;

  // 构造后不可更改
  Comparator const compare_;
  Arena* const arena_;  // 用于node内存的分配
  
  Node* const head_;

  // 只能被insert()改变
  std::atomic<int> max_height_;  // 调表的高度

  Random rnd_;
};

// 实现细节
template<typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {
  explicit Node(const Key& k) : key(k) {}

  Key const key;

  Node* Next(int n) {
    assert(n >= 0);
    // Use an 'acquire load' so that we observe a fully initialized version of the returned Node.
    return next_[n].load(std::memory_order_acquire); // 用来修饰一个读操作，表示在本线程中，所有后续的关于此变量的内存操作都必须在本条原子操作完成后执行。
  }
  void SetNext(int n, Node* x) {
    assert(n >= 0);
    // Use a 'release store' so that anybody who reads through this pointer observes a fully initialized version of the inserted node.
    next_[n].store(x, std::memory_order_release); // 用来修饰一个写操作，表示在本线程中，所有之前的针对该变量的内存操作完成后才能执行本条原子操作。
  }

  // No-barrier variants that can be safely used in a few locations.
  Node* NoBarrier_Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrier_SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_relaxed); // 针对一个变量的读写操作是原子操作；不同线程之间针对该变量的访问操作先后顺序不能得到保证，即有可能乱序。
  }

 private:
  // 数组的长度等于节点的高度，next_[0]就是最低层
  std::atomic<Node*> next_[1];
};

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key, Comparator>::NewNode(
    const Key& key, int height) {
  char* const node_memory = arena_.AllocateAligned(
      sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
  return new (node_memory) Node(key);

}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template <typename Key, class Comparator>
inline void  SkipList<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  return node_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  assert(Valid());
  node_ = FindLessThan(node_->key);
  if(node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = FindGreaterOrEqual(key, nullptr);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast() {
  node_ = FindLast();
  if(node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight() {
  static const unsigned int kBranching = 4;
  int height = 1;
  while(height < kMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key, 
                                              Node** prev) const {
  Node* x = head_;
  int level = GetMaxHight() - 1;
  while(true) {
    Node* next = x->Next(level);
    if(KeyIsAfterNode(key, next)) {
      x = next;
    } else {
      if(prev != nullptr) prev[level] = x;
      if(level == 0) {
        return next;
      } else {
        level--;
      }
    }
  }      
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHight()-1;
  while(true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if(next == nullptr || compare_(next->key, key) >= 0) {
      if(level == 0) {
        return x;
      } else {
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHight() - 1;
  while(true) {
    Node* next = x->Next(level);
    if(next == nullptr) {
      if(level == 0) {
        return x;
      } else {
        level--;
      }
    } else {
      x = next;
    }
  }

}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0 /* any key will do */, kMaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) /* 已分配但还未初始化的内存中用该数字来填充 */ {
        for(int i = 0; i < kMaxHeight; i++) {
          head_->SetNext(i, nullptr);
        }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key) {
  Node* prev[kMaxHeight];
  Node* x = FindGreaterOrEqual(key, prev);

  assert(x == nullptr || !Equal(key, x->key)); // 在插入的时候要么找不到节点， 要么找到的节点key与插入的不同

  int height = RandomHeight();
  if(height > GetMaxHight()) {
    for(int i = GetMaxHight(); i < height; i++) {
      prev[i] = head_;
    }
    // 可以不使用任何同步方式来改变 max_height_ 
    // 在并发读取的时候，有两种情况，第一种读到的是nullptr指针，第二种是新节点的指针
    // 前一种情况直接level--，后一种情况就使用新节点，
    max_height_.store(height, std::memory_order_relaxed);
  }

  x = NewNode(key, height);
  for(int i = 0; i < height; i++) {
    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i)); // 插入x时我们不会访问x-》next所以不需要屏障
    prev[i]->SetNext(i, x); // 设置 x的pre的next时，在多线程情况下可能存在使用x的pre的next 所以我们设置时使用SetNext，保证设置时，前面针对pre指针的操作都执行完毕
  }
}
template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return flase;
  }
}

} // namespace leveldb

#endif  //STORAGE_LEVELDB_DB_SKIPLIST_H_
