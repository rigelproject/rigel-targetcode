#include <sys/mman.h>
#include "rigel.h"

extern uintptr_t malloc_arena_start;
extern uintptr_t malloc_arena_end;

static uintptr_t malloc_arena_top = 0;

void*   mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off) {
if(malloc_arena_top > malloc_arena_end)
  return MAP_FAILED;
//Let's only allocate in increments of a cache line.
if(malloc_arena_top == 0)
  malloc_arena_top = malloc_arena_start;
uintptr_t ret = malloc_arena_top;
malloc_arena_top += ((len + 31) & (~(0x1F)));
if(malloc_arena_top > malloc_arena_end || malloc_arena_top < malloc_arena_start)
  return MAP_FAILED;
else
  return ((void *)ret);
}

//FIXME munmap() is called by malloc when releasing
//memory back to the "OS" or when destroying an mspace,
//like at the end of a program or when exit() is called.
//Our current dummy implementation always returns 0 (success),
//but this is misleading because we're not keeping track of memory,
//so that memory just "disappears" at that point.
//FIXME We need some code that keeps track of which chunks are allocated
//and which aren't; maybe we could use it for a SASOS type thing,
//and multiple apps could each have their own allocators
int     munmap(void *addr, size_t len) {
return 0;
}

int     mprotect(void *addr, size_t len, int prot) {
RigelAbort();
return 0;
}

int     msync(void *addr, size_t len, int flags) {
RigelAbort();
return 0;
}

int     mlock(const void *addr, size_t len) {
RigelAbort();
return 0;
}

int     munlock(const void *addr, size_t len) {
RigelAbort();
return 0;
}

