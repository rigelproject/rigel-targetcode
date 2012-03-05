#include <pthread.h> //For function prototypes and typedefs
#include <errno.h>   //For errno macro definitions
#include <stdlib.h>  //For malloc
#include <string.h> //For memcpy
#include <stddef.h>  //For NULL
#include <rigel.h>   //For RigelGetThreadNum()
#include "rigellib.h" //For __CACHE_LINE_SIZE_BYTES

static void broadcast_update(void *addr, void *val) {
  __asm__ __volatile__ ("bcast.u %0, %1, 0;\n" \
                      :                        \
                      : "r" (addr), "r" (val)  \
                      : "memory");
}

static inline void global_store(const void *addr, const void *val)
{
  __asm__ __volatile__ ("g.stw %1, %0, 0" \
                        :                 \
                        : "r" (addr),     \
                          "r" (val)       \
                        : "memory");
}

static inline void *global_load(const void *addr)
{
  void *ret;
  __asm__ __volatile__ ("g.ldw %0, %1, 0" \
      : "=r" (ret)      \
      : "r"  (addr));
  return ret;
}

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) {
  if(a != NULL)
    return -EINVAL; //Don't handle condattrs yet.
  if(c->__data.__broadcast == 1)
    return -EINVAL;
  c->__data.__broadcast = 1; //Init
  c->__data.__mutex = NULL;
  RigelFlushLine(&c->__data.__broadcast);
  RigelFlushLine(&c->__data.__mutex);
  return 0;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
  void *cur_mutex_val = global_load((void *)&c->__data.__mutex);
  if(cur_mutex_val == NULL)
    global_store((void *)&c->__data.__mutex, (void *)m);
  else
    if(cur_mutex_val != (void *)m)
      return -EINVAL; //Mutexes must match for all waiters.
  if(pthread_mutex_unlock(m) != 0)
    return -EINVAL;
  while(c->__data.__broadcast != 2); //Spin locally until wakeup broadcast is received
  RigelFlushLine(c->__data.__broadcast);
  if(pthread_mutex_lock(m) != 0)
    return -EINVAL;
  return 0;
}

int pthread_cond_signal(pthread_cond_t *c) {
  return pthread_cond_broadcast(c);
}
int pthread_cond_broadcast(pthread_cond_t *c) {
  if(c->__data.__broadcast != 1)
    return -EINVAL;
  RigelFlushLine(&c->__data.__broadcast);
  broadcast_update((void *)&c->__data.__broadcast, (void *)2);
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *c) {
  c->__data.__broadcast = 0;
  c->__data.__mutex = NULL;
  return 0;
}