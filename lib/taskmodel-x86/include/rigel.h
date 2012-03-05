#ifndef __RIGEL_H__
#define __RIGEL_H__
// Note that this is the **x86** version of the rigel include file!
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "./rigel-memops.h"

// Returns the core number represented by the thread.  Defined in librtm-x86.c
int RigelGetCoreNum();
// Return the cluster number of the thread.
int RigelGetClusterNum();
// Return the number of cores present in the machine
int RigelGetNumCores();

// Timer library TODO
void ClearTimer(int timer_num);
void StartTimer(int timer_num);
void StopTimer(int timer_num);

#define __CACHE_LINE_SIZE 64

typedef volatile struct __atomic_flag_t {
	volatile unsigned int flag_val;
	unsigned char padding[__CACHE_LINE_SIZE - sizeof(unsigned int)];
} atomic_flag_t __attribute__ ((aligned (8)));

#define RigelPrint(x) do {                \
  fprintf(stderr, "PRINT: 0x%08x (%f)\n", \
    (unsigned int)(x),             \
    (float)(x)                    \
  ); } while (0);

#define RigelAbort() do {     \
    fprintf(stderr, "!!!RIGELABORT!!! %s @ %d in %s Abort.\n",  \
      __FUNCTION__,                                             \
      __LINE__,                                                 \
      __FILE__                                                  \
    );                                                          \
    assert(0);                                                  \
  } while (0);

// Define flag states
#define ATOMICFLAG_INIT_SET(foo) \
	atomic_flag_t foo = { 1 };
#define ATOMICFLAG_INIT_CLEAR(foo) \
	atomic_flag_t foo = { 0 };

#define RigelAtomicADD(i, val) do { \
  int __i = i;                      \
  asm volatile (                    \
    "lock; xaddl %0,%1"             \
    : "+r" (i), "+m" (val)         \
    : : "memory" );                 \
} while (0);

#define RigelAtomicINC(i, val) do { \
  i = 1;                            \
  RigelAtomicADD(i, val);           \
  fprintf(stderr, "FOUND atom inc of val: %d res: %d\n", val, i); \
} while (0);

void atomic_flag_set(atomic_flag_t *f);
void atomic_flag_spin_until_set(atomic_flag_t *f);
void atomic_flag_spin_until_clear(atomic_flag_t *f);

size_t
ffilemap(void *buf, size_t size, size_t nobj, FILE* fp);
#endif
