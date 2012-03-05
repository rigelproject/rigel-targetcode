#include <stdlib.h>
#include <stdint.h>
#include <rigel.h>
#include <errno.h>
#include <pthread.h>

#define __MAX_KEYS_PER_THREAD 32
#define PTHREAD_MAX_DESTRUCTOR_ITERATIONS 4

typedef struct
{
        uint32_t valid;
        void (*destructors[__MAX_KEYS_PER_THREAD])(void *); //Array of __MAX_KEYS_PER_THREAD pointers to
                                                            //functions that return void and take a void*
        void *data[__MAX_KEYS_PER_THREAD];
} __pthread_tls_t;

static __pthread_tls_t pthread_tls[__MAX_NUM_THREADS] = {0};

int pthread_key_create(pthread_key_t *key, void(*destructor)(void *))
{
  __pthread_tls_t * const tls = &(pthread_tls[RigelGetThreadNum()]);
  for(unsigned int i = 0; i < __MAX_KEYS_PER_THREAD; i++)
  {
    const unsigned int mask = 1U << i;
    if(!(tls->valid & mask))
    {
      tls->valid |= mask;
      tls->destructors[i] = destructor;
      *key = i;
      return 0;
    }
  }
  return -EAGAIN; //Out of keys.
}

int pthread_key_delete(pthread_key_t key)
{
  if(key > __MAX_KEYS_PER_THREAD)
    return -EINVAL;
  __pthread_tls_t * const tls = &(pthread_tls[RigelGetThreadNum()]);
  const unsigned int mask = 1U << key;
  if(!(tls->valid & mask))
    return -EINVAL;
  tls->valid &= ~mask;
  unsigned int tries = 0;
  while((tls->data[key] != NULL) && (tries < PTHREAD_MAX_DESTRUCTOR_ITERATIONS)) {
    tls->destructors[key](tls->data[key]);
    tries++;
  }
  return 0;
}

//NOTE: I'm omitting the key checks in get/setspecific b/c they're not
//required by the standard (behavior is undefined).
void *pthread_getspecific(pthread_key_t key)
{
  //if(key > __MAX_KEYS_PER_THREAD)
  //  return -EINVAL;
  __pthread_tls_t * const tls = &(pthread_tls[RigelGetThreadNum()]);
  //const unsigned int mask = 1U << key;
  //if(!(tls->valid & (1U << key)))
  //  return -EINVAL;
  return tls->data[key];
}

int pthread_setspecific(pthread_key_t key, const void *val)
{
  //if(key > __MAX_KEYS_PER_THREAD)
  //  return -EINVAL;
  __pthread_tls_t * const tls = &(pthread_tls[RigelGetThreadNum()]);
  //const unsigned int mask = 1U << key;
  //if(!(tls->valid & (1U << key)))
  //  return -EINVAL;
  tls->data[key] = (void *)val;
  return 0;
}

