#include <pthread.h> //For function prototypes and typedefs
#include <errno.h>   //For errno macro definitions
#include <string.h> //For memcpy
#include <stddef.h>  //For NULL
#include <rigel.h>   //For RigelGetThreadNum()

#define __PTHREAD_MUTEX_GETTYPE(kind) ((kind) & 0x3)
#define __PTHREAD_MUTEX_SETTYPE(attr, type) do { (attr).__align |= (type); } while(0);

//TODO I'd rather not have these all over the place, I'd like them all in one place.
//Having them in rigel.h means (I think) that you can't have them as static inline functions
//(or can you?), but macros are totally the wrong way to do it if you can inline the function.
//Another way to do it might be to make them LLVM intrinsics.
static inline int compare_and_swap(const void *ptr, const void *compareval, void *swapval)
{
  __asm__ __volatile__ ("atom.cas %0, %2, %1" \
                        : "+&r" (swapval)     \
                        : "r"   (ptr),        \
                          "r"   (compareval)  \
                        : "memory");
  return (swapval == compareval);
}

static inline void *swap(const void *ptr, void *swapval)
{
  __asm__ __volatile__ ("atom.xchg %0, %1, 0" \
                        : "+&r" (swapval)     \
                        : "r"  (ptr)          \
                        : "memory");
  return swapval;
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

static inline unsigned int atomic_inc(const void *addr)
{
  unsigned int ret;
  __asm__ __volatile__ ("atom.inc %0, %1, 0" \
                        : "=r" (ret)         \
                        : "r"  (addr)        \
                        : "memory");
  return ret;
}

static inline unsigned int atomic_dec(const void *addr)
{
  unsigned int ret;
  __asm__ __volatile__ ("atom.dec %0, %1, 0" \
                        : "=r" (ret)         \
                        : "r"  (addr)        \
                        : "memory");
  return ret;
}

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
  if(a == NULL)
    m->__data.__kind = 1 << 15;
  else
  {
    if(!(a->__align & (1 << 15)))
      return -EINVAL; //a is uninitialized.
    else
      memcpy(&(m->__data.__kind), &(a->__size), __SIZEOF_PTHREAD_MUTEXATTR_T);
  }
  switch(__PTHREAD_MUTEX_GETTYPE(m->__data.__kind)) {
  	case PTHREAD_MUTEX_RECURSIVE:
  	  m->__data.__count = 0;
      RigelFlushLine(&(m->__data.__count)); //Need separate flushes because we don't guarantee pthread_mutex_t is line-aligned.
  	  //Fallthrough
  	case PTHREAD_MUTEX_ERRORCHECK:
      m->__data.__owner = -1;
      RigelFlushLine(&(m->__data.__owner));
      //Fallthrough
  	case PTHREAD_MUTEX_NORMAL:
  	  m->__data.__lock = 0;
      RigelFlushLine(&(m->__data.__lock));
      break;
    default:
      return -EINVAL;
  }
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m) {
  switch(__PTHREAD_MUTEX_GETTYPE(m->__data.__kind)) {
    case PTHREAD_MUTEX_NORMAL: //No error checking
      while((int)swap((void *)&m->__data.__lock, (void *)1) != 0);
      return 0;
      break;
    case PTHREAD_MUTEX_ERRORCHECK: {
      int tid = RigelGetThreadNum();
      if((int)global_load((void *)&m->__data.__owner) == tid) return -EDEADLK;
      while((int)swap((void *)&m->__data.__lock, (void *)1) != 0);
      global_store((void *)&m->__data.__owner, (void *)tid);
      return 0;
      break;
    }
    case PTHREAD_MUTEX_RECURSIVE:
    {
      int tid = RigelGetThreadNum();
      if(compare_and_swap((void *)&m->__data.__count, (void *)0, (void *)1)) { //If m->__data.__count == 0 (unowned), take it
        global_store(&m->__data.__owner, (void *)tid);
      }
      else {
        if((int)global_load(&m->__data.__owner) == tid) { //We already own it
          (void)atomic_inc(&m->__data.__count);
        }
        else { //Spin until we get it
          while((int)swap((void *)&m->__data.__lock, (void *)1) != 0);
          global_store((void *)&m->__data.__owner, (void *)tid);
        }
      }
      return 0;
      break;
    }
    default:
      return -EINVAL;
  }
  return -EINVAL;
}

int pthread_mutex_trylock(pthread_mutex_t *m) {
  switch(__PTHREAD_MUTEX_GETTYPE(m->__data.__kind)) {
    case PTHREAD_MUTEX_NORMAL: //No error checking
      if((int)swap((void *)&m->__data.__lock, (void *)1) != 0)
        return -EBUSY;
      else
        return 0;
      break;
    case PTHREAD_MUTEX_ERRORCHECK:
    {
      int tid = RigelGetThreadNum();
      if((int)global_load((void *)&m->__data.__owner) == tid) return -EDEADLK;
      if((int)swap((void *)&m->__data.__lock, (void *)1) != 0)
        return -EBUSY;
      else {
        global_store((void *)&m->__data.__owner, (void *)tid);
        return 0;
      }
      break;
    }
    case PTHREAD_MUTEX_RECURSIVE:
    {
      int tid = RigelGetThreadNum();
      if(compare_and_swap((void *)&m->__data.__count, (void *)0, (void *)1)) { //If m->__data.__count == 0 (unowned), take it
        global_store(&m->__data.__owner, (void *)tid);
      }
      else {
        if((int)global_load(&m->__data.__owner) == tid) { //We already own it
          (void)atomic_inc(&m->__data.__count);
        }
        else { //Spin until we get it
          if((int)swap((void *)&m->__data.__lock, (void *)1) != 0)
            return -EBUSY;
          else {
            global_store((void *)&m->__data.__owner, (void *)tid);
            return 0;
          }
        }
      }
      break;
    }
    default:
      return -EINVAL;
  }
  return -EINVAL;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
  switch(__PTHREAD_MUTEX_GETTYPE(m->__data.__kind)) {
    case PTHREAD_MUTEX_NORMAL: //No error checking
      global_store((void *)&m->__data.__lock, (void *)0);
      return 0;
      break;
    case PTHREAD_MUTEX_ERRORCHECK:
    {
      int tid = RigelGetThreadNum();
      if((int)global_load((void *)&m->__data.__owner) == tid) return -EPERM;
      global_store((void *)&m->__data.__owner, (void *)0);
      global_store((void *)&m->__data.__lock, (void *)0);
      return 0;
      break;
    }
    case PTHREAD_MUTEX_RECURSIVE:
    {
      int tid = RigelGetThreadNum();
      if((int)global_load(&m->__data.__owner) != tid)
        return -EPERM;
      if((int)global_load(&m->__data.__count) == 1) //atomic_dec() will release the lock
        global_store(&m->__data.__owner, (void *)-1); //null out the owner first
      atomic_dec(&m->__data.__count);
      return 0;
      break;
    }
    default:
      return -EINVAL;
  }
  return -EINVAL;
}

int   pthread_mutexattr_init(pthread_mutexattr_t *a) {
  a->__align = 1 << 15; //Use bit 15 to indicate initializedness
  return 0;
}

int   pthread_mutexattr_destroy(pthread_mutexattr_t *a) {
  a->__align = 0;
  return 0;
}

int   pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) {
  if(type < PTHREAD_MUTEX_NORMAL || type > PTHREAD_MUTEX_RECURSIVE)
    return -EINVAL;
  if(a == NULL || !(a->__align & (1 << 15)))
    return -EINVAL;
  __PTHREAD_MUTEX_SETTYPE(*a, type);
  return 0;
}

int   pthread_mutexattr_gettype(const pthread_mutexattr_t *a, int *type) {
  if(a == NULL || !(a->__align & (1 << 15)))
    return -EINVAL;
  *type = a->__align & 0x3;
  return 0;
}