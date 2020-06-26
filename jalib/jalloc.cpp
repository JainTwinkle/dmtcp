/****************************************************************************
 *   Copyright (C) 2006-2008 by Jason Ansel                                 *
 *   jansel@csail.mit.edu                                                   *
 *                                                                          *
 *   This file is part of the JALIB module of DMTCP (DMTCP:dmtcp/jalib).    *
 *                                                                          *
 *  DMTCP:dmtcp/jalib is free software: you can redistribute it and/or      *
 *  modify it under the terms of the GNU Lesser General Public License as   *
 *  published by the Free Software Foundation, either version 3 of the      *
 *  License, or (at your option) any later version.                         *
 *                                                                          *
 *  DMTCP:dmtcp/src is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU Lesser General Public License for more details.                     *
 *                                                                          *
 *  You should have received a copy of the GNU Lesser General Public        *
 *  License along with DMTCP:dmtcp/src.  If not, see                        *
 *  <http://www.gnu.org/licenses/>.                                         *
 ****************************************************************************/

#include "config.h" /* For HAS_ATOMIC_BUILTINS */
#include "jalib.h"
#include "jalloc.h"
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Make highest chunk size large; avoid a raw_alloc calling mmap()
// during /proc/self/maps
#define MAX_CHUNKSIZE (4 * 1024)

using namespace jalib;

extern "C" int fred_record_replay_enabled() __attribute__((weak));
static bool _initialized = false;

#ifdef JALIB_ALLOCATOR

# include <stdlib.h>
# include <sys/mman.h>

namespace jalib
{
# ifndef HAS_ATOMIC_BUILTINS

// We'll use critical section instead of atomic builtins.
// Hopefully, all changes to the variables go through this critical section

static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;

template<typename T>
static bool
__sync_bool_compare_and_swap(
  T volatile *ptr, T oldval, T newval)
{
  bool retval = false;
  jalib::pthread_mutex_lock(&sync_mutex);

  if (*ptr == oldval) {
    *ptr = newval;
    retval = true;
  }
  jalib::pthread_mutex_unlock(&sync_mutex);
  return retval;
}
# endif // ifndef HAS_ATOMIC_BUILTINS

inline void *
_alloc_raw(size_t n)
{
# ifdef JALIB_USE_MALLOC
  return malloc(n);

# else // ifdef JALIB_USE_MALLOC

  // #define USE_DMTCP_ALLOC_ARENA
#  ifdef USE_DMTCP_ALLOC_ARENA
#   ifndef __x86_64__
#    error "USE_DMTCP_ALLOC_ARENA can't be used with 32-bit binaries"
#   endif // ifndef __x86_64__
  static void *mmapHintAddr = (void *)0x1f000000000;
  if (n % sysconf(_SC_PAGESIZE) != 0) {
    n = (n + sysconf(_SC_PAGESIZE) - (n % sysconf(_SC_PAGESIZE)));
  }
  void *p = mmap(mmapHintAddr, n, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (p != MAP_FAILED) {
    mmapHintAddr = p + n;
  }
#  else // ifdef USE_DMTCP_ALLOC_ARENA
  void *p = mmap(NULL,
                 n,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1,
                 0);
#  endif // ifdef USE_DMTCP_ALLOC_ARENA

  if (p == MAP_FAILED) {
    perror("DMTCP(" __FILE__ "): _alloc_raw: ");
  }
  return p;
# endif // ifdef JALIB_USE_MALLOC
}

inline void
_dealloc_raw(void *ptr, size_t n)
{
# ifdef JALIB_USE_MALLOC
  free(ptr);
# else // ifdef JALIB_USE_MALLOC
  if (ptr == 0 || n == 0) {
    return;
  }
  int rv = munmap(ptr, n);
  if (rv != 0) {
    perror("DMTCP(" __FILE__ "): _dealloc_raw: ");
  }
# endif // ifdef JALIB_USE_MALLOC
}

/*
 * Atomically compares the word at the given 'oldValue' address with the
 * word at the given 'dst' address. If the two words are equal, the word
 * at 'dst' address is changed to the word at 'newValue' address.
 * The function returns a non-zero value if the exchange was successful.
 * If the exchange is not successfully, the function returns 0.
 */
static inline uint8_t
bool_atomic_dwcas(void volatile *dst, void *oldValue, void *newValue)
{
  uint8_t result = 0;
#ifdef __x86_64__
  /*
   * Pointers are 64-bits and so we can't really use the built-in
   * __sync_bool_compare_and_swap here because gcc only supports 8
   * Byte compare-and-swap. So, we need to hand roll a version of
   * compare-and-swap using "cmpxchg16b" that can handle 16 Bytes at a
   * time. For 32-bit systems, we can continue to use the built-in 64-bit
   * (8 Byte) compare-and-swap.
   */

  // Useful type definition for inline assembly below
  struct uint128_t {
    uint64_t low_word;
    uint64_t high_word;
  };
  asm volatile (
    "lock cmpxchg16b %1;"       // cmpxchg16b compares RDX:RAX with m128,
                                //   if equal,
                                //     sets ZF and loads RCX:RBX into m128
                                //   else,
                                //     clears ZF and loads RDX:RAX into m128
    "setz %0"                   // Set result to 1, if ZF is set
    : // output
      "=q" (result), // Use any register with addressable lower byte
      "+m" (*(uint128_t*)dst),  // Output goes to memory
      "+d" (((uint128_t*)oldValue)->high_word), // Use D register for higher 64 bits
      "+a" (((uint128_t*)oldValue)->low_word)   // Use A register for lower 64 bits
    : // input
      "c" (((uint128_t*)newValue)->high_word),  // Use C register for higher 64 bits
      "b" (((uint128_t*)newValue)->low_word)    // Use B register for lower 64 bits
    : // clobber
  );
#elif __arm__ || __i386__
  result = __sync_bool_compare_and_swap((uint64_t volatile *)dst,
                                        *(uint64_t*)oldValue,
                                        *(uint64_t*)newValue);
#elif __aarch64__
  result = __atomic_compare_exchange((__int128*)dst,
                                     (__int128*)oldValue,
                                     (__int128*)newValue, 0,
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif /* if __x86_64__ */
  return result;
}

template<size_t _N>
class JFixedAllocStack
{
  public:
    enum { N = _N };
    JFixedAllocStack()
    {
      if (_blockSize == 0) {
        _blockSize = 4 * MAX_CHUNKSIZE;
      }
      memset(&_top, 0, sizeof(_top));
      _numExpands = 0;
    }

    void initialize(int blockSize)
    {
      _blockSize = blockSize;
    }

    size_t chunkSize() { return N; }

    // allocate a chunk of size N
    void *allocate()
    {
      StackHead origHead = {0};
      StackHead newHead = {0};

      do {
        origHead = _top;
        if (origHead.node == NULL) {
          expand();
          origHead = _top;
        }

        // NOTE: _root could still be NULL (if other threads consumed all
        // blocks that were made available from expand().  In such case, we
        // loop once again.

        /* Atomically does the following operation:
         *   origHead = _top;
         *   _top = origHead.node->next;
         */
        // origHead.node is guaranteed to be a valid value if we get here
        newHead.node = origHead.node->next;
        newHead.counter = origHead.counter + 1;
      } while (!origHead.node ||
               !bool_atomic_dwcas(&_top, &origHead, &newHead));

      origHead.node->next = NULL;
      return origHead.node;
    }

    // deallocate a chunk of size N
    void deallocate(void *ptr)
    {
      if (ptr == NULL) { return; }
      FreeItem *item = static_cast<FreeItem *>(ptr);
      StackHead origHead = {0};
      StackHead newHead = {0};
      do {
        /* Atomically does the following operation:
         *   item->next = _top.node;
         *   _top = newHead;
         */
        origHead = _top;
        item->next = origHead.node;
        newHead.counter = origHead.counter + 1;
        newHead.node = item;
      } while (!bool_atomic_dwcas(&_top, &origHead, &newHead));
    }

    int numExpands()
    {
      return _numExpands;
    }

    void preExpand()
    {
      // Force at least numChunks chunks to become free.
      const int numAllocs = 10;
      void *allocatedItem[numAllocs];

      for (int i = 0; i < numAllocs; i++) {
        allocatedItem[i] = allocate();
      }

      // if (_root == NULL) { expand(); }
      for (int i = 0; i < numAllocs; i++) {
        deallocate(allocatedItem[i]);
      }
    }

  protected:
    // allocate more raw memory when stack is empty
    void expand()
    {
      StackHead origHead = {0};
      StackHead newHead = {0};
      _numExpands++;
      if (_top.node != NULL &&
          fred_record_replay_enabled && fred_record_replay_enabled()) {
        // TODO: why is expand being called? If you see this message, raise lvl2
        // allocation level.
        char expand_msg[] = "\n\n\n******* EXPAND IS CALLED *******\n\n\n";
        jalib::write(2, expand_msg, sizeof(expand_msg));

        // jalib::fflush(stderr);
        abort();
      }
      FreeItem *bufs = static_cast<FreeItem *>(_alloc_raw(_blockSize));
      int count = _blockSize / sizeof(FreeItem);
      for (int i = 0; i < count - 1; ++i) {
        bufs[i].next = bufs + i + 1;
      }

      do {
        /* Atomically does the following operation:
         *   bufs[count - 1].next = _top.node;
         *   _top = bufs;
         */
        origHead = _top;
        bufs[count - 1].next = origHead.node;
        newHead.node = bufs;
        newHead.counter = origHead.counter + 1;
      } while (!bool_atomic_dwcas(&_top, &origHead, &newHead));
    }

  protected:
    struct FreeItem {
      union {
        FreeItem *next;
        char buf[N];
      };
    };
    struct StackHead {
      uintptr_t counter;
      FreeItem* volatile node;
    };

  private:
    StackHead _top;
    size_t _blockSize = 0;
    char padding[128];
    int volatile _numExpands;
};
} // namespace jalib

jalib::JFixedAllocStack<64>lvl1;
jalib::JFixedAllocStack<256>lvl2;
jalib::JFixedAllocStack<1024>lvl3;
# if MAX_CHUNKSIZE <= 1024
#  error MAX_CHUNKSIZE must be larger
# endif // if MAX_CHUNKSIZE <= 1024
jalib::JFixedAllocStack<MAX_CHUNKSIZE>lvl4;

void
jalib::JAllocDispatcher::initialize(void)
{
  if (fred_record_replay_enabled != 0 && fred_record_replay_enabled()) {
    /* We need a greater arena size to eliminate mmap() calls that could happen
       at different times for record vs. replay. */
    lvl1.initialize(1024 * 1024 * 16);
    lvl2.initialize(1024 * 1024 * 16);
    lvl3.initialize(1024 * 32 * 16);
    lvl4.initialize(1024 * 32 * 16);
  } else {
    lvl1.initialize(1024 * 16);
    lvl2.initialize(1024 * 16);
    lvl3.initialize(1024 * 32);
    lvl4.initialize(1024 * 32);
  }
  _initialized = true;
}

void *
jalib::JAllocDispatcher::allocate(size_t n)
{
  if (!_initialized) {
    initialize();
  }
  void *retVal;
  if (n <= lvl1.chunkSize()) {
    retVal = lvl1.allocate();
  } else if (n <= lvl2.chunkSize()) {
    retVal = lvl2.allocate();
  } else if (n <= lvl3.chunkSize()) {
    retVal = lvl3.allocate();
  } else if (n <= lvl4.chunkSize()) {
    retVal = lvl4.allocate();
  } else {
    retVal = _alloc_raw(n);
  }
  return retVal;
}

void
jalib::JAllocDispatcher::deallocate(void *ptr, size_t n)
{
  if (!_initialized) {
    char msg[] = "***DMTCP INTERNAL ERROR: Free called before init\n";
    jalib::write(2, msg, sizeof(msg));
    abort();
  }
  if (n <= lvl1.N) {
    lvl1.deallocate(ptr);
  } else if (n <= lvl2.N) {
    lvl2.deallocate(ptr);
  } else if (n <= lvl3.N) {
    lvl3.deallocate(ptr);
  } else if (n <= lvl4.N) {
    lvl4.deallocate(ptr);
  } else {
    _dealloc_raw(ptr, n);
  }
}

int
jalib::JAllocDispatcher::numExpands()
{
  return lvl1.numExpands() + lvl2.numExpands() +
         lvl3.numExpands() + lvl4.numExpands();
}

void
jalib::JAllocDispatcher::preExpand()
{
  lvl1.preExpand();
  lvl2.preExpand();
  lvl3.preExpand();
  lvl4.preExpand();
}

#else // ifdef JALIB_ALLOCATOR

# include <stdlib.h>

void *
jalib::JAllocDispatcher::allocate(size_t n)
{
  void *p = ::malloc(n);

  return p;
}

void
jalib::JAllocDispatcher::deallocate(void *ptr, size_t)
{
  ::free(ptr);
}
#endif // ifdef JALIB_ALLOCATOR

#ifdef OVERRIDE_GLOBAL_ALLOCATOR
# ifndef JALIB_ALLOCATOR
#  error \
  "JALIB_ALLOCATOR must be #defined in dmtcp/jalib/jalloc.h for --enable-allocator to work"
# endif // ifndef JALIB_ALLOCATOR
void *
operator new(size_t nbytes)
{
  if (fred_record_replay_enabled && fred_record_replay_enabled()) {
    fprintf(stderr, "*** DMTCP Internal Error: OVERRIDE_GLOBAL_ALLOCATOR not"
                    " supported with FReD\n\n");
    abort();
  }
  size_t *p = (size_t *)jalib::JAllocDispatcher::allocate(
      nbytes + sizeof(size_t));
  *p = nbytes;
  p += 1;
  return p;
}

void
operator delete(void *_p)
{
  if (fred_record_replay_enabled && fred_record_replay_enabled()) {
    fprintf(stderr, "*** DMTCP Internal Error: OVERRIDE_GLOBAL_ALLOCATOR not"
                    " supported with FReD\n\n");
    abort();
  }
  size_t *p = (size_t *)_p;
  p -= 1;
  jalib::JAllocDispatcher::deallocate(p, *p + sizeof(size_t));
}
#endif // ifdef OVERRIDE_GLOBAL_ALLOCATOR
