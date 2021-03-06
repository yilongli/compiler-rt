//===-- hwasan_thread.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of HWAddressSanitizer.
//
//===----------------------------------------------------------------------===//

#ifndef HWASAN_THREAD_H
#define HWASAN_THREAD_H

#include "hwasan_allocator.h"
#include "sanitizer_common/sanitizer_common.h"

namespace __hwasan {

class Thread {
 public:
  static void Create();  // Must be called from the thread itself.
  void Destroy();

  uptr stack_top() { return stack_top_; }
  uptr stack_bottom() { return stack_bottom_; }
  uptr stack_size() { return stack_top() - stack_bottom(); }
  uptr tls_begin() { return tls_begin_; }
  uptr tls_end() { return tls_end_; }
  bool IsMainThread() { return unique_id_ == 0; }

  bool AddrIsInStack(uptr addr) {
    return addr >= stack_bottom_ && addr < stack_top_;
  }

  bool InSignalHandler() { return in_signal_handler_; }
  void EnterSignalHandler() { in_signal_handler_++; }
  void LeaveSignalHandler() { in_signal_handler_--; }

  bool InSymbolizer() { return in_symbolizer_; }
  void EnterSymbolizer() { in_symbolizer_++; }
  void LeaveSymbolizer() { in_symbolizer_--; }

  bool InInterceptorScope() { return in_interceptor_scope_; }
  void EnterInterceptorScope() { in_interceptor_scope_++; }
  void LeaveInterceptorScope() { in_interceptor_scope_--; }

  AllocatorCache *allocator_cache() { return &allocator_cache_; }
  HeapAllocationsRingBuffer *heap_allocations() {
    return heap_allocations_;
  }

  tag_t GenerateRandomTag();

  int destructor_iterations_;
  void DisableTagging() { tagging_disabled_++; }
  void EnableTagging() { tagging_disabled_--; }
  bool TaggingIsDisabled() const { return tagging_disabled_; }

  template <class CB>
  static void VisitAllLiveThreads(CB cb) {
    SpinMutexLock l(&thread_list_mutex);
    Thread *t = thread_list_head;
    while (t) {
      cb(t);
      t = t->next_;
    }
  }

  u64 unique_id() const { return unique_id_; }
  void Announce() {
    if (announced_) return;
    announced_ = true;
    Print("Thread: ");
  }

  struct ThreadStats {
    uptr n_live_threads;
    uptr total_stack_size;
  };

  static ThreadStats GetThreadStats() {
    SpinMutexLock l(&thread_list_mutex);
    return thread_stats;
  }

  static uptr MemoryUsedPerThread();

 private:
  // NOTE: There is no Thread constructor. It is allocated
  // via mmap() and *must* be valid in zero-initialized state.
  void Init();
  void ClearShadowForThreadStackAndTLS();
  void Print(const char *prefix);
  uptr stack_top_;
  uptr stack_bottom_;
  uptr tls_begin_;
  uptr tls_end_;

  unsigned in_signal_handler_;
  unsigned in_symbolizer_;
  unsigned in_interceptor_scope_;

  u32 random_state_;
  u32 random_buffer_;

  AllocatorCache allocator_cache_;
  HeapAllocationsRingBuffer *heap_allocations_;

  static void InsertIntoThreadList(Thread *t);
  static void RemoveFromThreadList(Thread *t);
  Thread *next_;  // All live threads form a linked list.
  static SpinMutex thread_list_mutex;
  static Thread *thread_list_head;
  static ThreadStats thread_stats;

  u64 unique_id_;  // counting from zero.

  u32 tagging_disabled_;  // if non-zero, malloc uses zero tag in this thread.

  bool announced_;
};

Thread *GetCurrentThread();
void SetCurrentThread(Thread *t);

struct ScopedTaggingDisabler {
  ScopedTaggingDisabler() { GetCurrentThread()->DisableTagging(); }
  ~ScopedTaggingDisabler() { GetCurrentThread()->EnableTagging(); }
};

} // namespace __hwasan

#endif // HWASAN_THREAD_H
