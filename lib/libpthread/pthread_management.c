#ifndef LLVM28
#error Need LLVM 2.8 to support weak functions
#endif

#include <pthread.h> //For function prototypes and typedefs
#include <errno.h>   //For errno macro definitions
#include <stddef.h>  //For NULL
#include <stdio.h>   //For printf
#include "rigel.h"   //For intrinsics
#include "rigellib.h"//For __MAX_NUM_CORES, __CACHE_LINE_SIZE_BYTES

#define ___MAX_THREADS_PER_CORE 4

//Only 1 concurrent Pthread allowed per hardware thread.
//pthread_create() returns EAGAIN if all HW threads are busy.
//Given this limitation, a thread's TID (pthread_t tid) is
//always equivalent to its global hardware TID.  This allows
//us to implement join() very efficiently without a
//pthread-tid-to-hardware-tid mapping data structure.

typedef struct __attribute__ ((aligned (__CACHE_LINE_SIZE_BYTES) )) __per_thread_pthread_data_t {
  pthread_t tid;
  pthread_t callingtid;
  void *(*funcptr) (void *);
  void *funcarg;
  void *retval;
} per_thread_pthread_data_t;

static volatile per_thread_pthread_data_t _pt[__MAX_NUM_CORES*___MAX_THREADS_PER_CORE]; //TODO: Implement __MAX_NUM_THREADS (or better, figure out how to malloc this at runtime).

void __rigel_premain(void)
{
  int tid = RigelGetThreadNum();
  if(tid == 0) {
    SIM_SLEEP_ON();
    SIM_SLEEP_OFF();
    _pt[tid].funcptr = (void *)0xFFFFFFFF; //Make sure we don't get a thread spawned on us by accident
  }
  else {
    RigelPrint(0xFEDC);
    while(1) {
      while(_pt[tid].funcptr == NULL);
      void *retval = _pt[tid].funcptr(_pt[tid].funcarg);
      RigelBroadcastUpdate(retval, _pt[tid].retval);
      RigelBroadcastUpdate(NULL, _pt[tid].funcptr);
    }
  }
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
  for(int i = 0; i < (__MAX_NUM_CORES*___MAX_THREADS_PER_CORE); i++) {
    if(_pt[i].funcptr == NULL) { //Thread is available
      *thread = i;
      RigelBroadcastUpdate(arg, _pt[i].funcarg);
      RigelBroadcastUpdate(_pt[RigelGetThreadNum()].tid, _pt[i].callingtid);
      RigelBroadcastUpdate(i, _pt[i].tid);
      RigelBroadcastUpdate(start_routine, _pt[i].funcptr); //The thread will be polling on this, so it should be the last thing to update.
      return 0;
    }
  }
  //No threads are available.
  return EAGAIN;
}

//TODO: Need some global structure to track joiners, in order to:
//1) Detect deadlock (2 thread try to join() each other)
//2) Detect if multiple threads are trying to join() the same thread
//Both of these are required for this function by the POSIX standard.
int pthread_join(pthread_t thread, void **retval)
{
  if(thread == RigelGetThreadNum()) return EDEADLK;
  if(thread >= RigelGetNumThreads()) return ESRCH;
  while(_pt[thread].funcptr != NULL) {
  //spin
  }
  if(retval != NULL) {
    *retval = _pt[thread].retval;
  }
  return 0;
}
