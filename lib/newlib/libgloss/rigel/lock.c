//#include <spinlock.h>
//#include <stdio.h>
#include <rigel.h>
#include <stdint.h>
#include <sys/lock.h>

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

void _lock_init(_LOCK_T *lock) {
  global_store(lock, (void *)0);
}

void _lock_init_recursive(_LOCK_RECURSIVE_T *lock) {
  global_store(&(lock->owner), (void *)-1);
  global_store(&(lock->count), (void *)0);
}

int _lock_try_acquire(_LOCK_T *lock) {
  unsigned int swapval = 1;
  __asm__ __volatile__ ( "atom.xchg %0, %1, 0\n" : "+&r"(swapval) : "r"(lock) : "memory" );
  return (swapval == 0);
}

//Returns 1 on success, 0 on failure
int _lock_try_acquire_recursive(_LOCK_RECURSIVE_T *l)
{
  const unsigned int tid = RigelGetThreadNum();
  if(compare_and_swap(&(l->count), (void *)0, (void *)1)) { //If l->count == 0 (unowned), take it
    global_store(&(l->owner), (void *)tid);
		//RigelPrint(5);
		//RigelPrint(l);
    return 1;
  }
  else if(((unsigned int)global_load(&(l->owner))) == tid) { //We already own it
    atomic_inc(&(l->count));
		//RigelPrint(5);
		//RigelPrint(l);
    return 1;
  }
  else
    return 0;
}

void _lock_acquire(_LOCK_T *lock) {
  //RigelPrint(3);
  unsigned int swapval = 1;
  __asm__ __volatile__ ( "1:\n"
                         "atom.xchg %0, %1, 0\n"
                         "beq %0, $zero, 1b\n"
                         : "+&r"(swapval)
                         : "r"(lock)
                         : "memory"
                       );
}

//Assume that recursive locking is an uncommon case, so check count before owner
void _lock_acquire_recursive(_LOCK_RECURSIVE_T *s) {
  //RigelPrint(1);
	//RigelPrint(s);
  const unsigned int tid = RigelGetThreadNum();
  if(compare_and_swap(&(s->count), (void *)0, (void *)1)) { //If l->count == 0 (unowned), take it
    global_store(&(s->owner), (void *)tid);
    return;
  }
  else {
    if(((unsigned int)global_load(&(s->owner))) == tid) { //We already own it
      atomic_inc(&(s->count));
      return;
    }
    else { //Spin until we get it
      while(!compare_and_swap(&(s->count), (void *)0, (void *)1));
      global_store(&(s->owner), (void *)tid);
    }
  }
  return;
}

int _lock_release(_LOCK_T *l)
{
  //RigelPrint(4);
  global_store(l, (void *)0);
  return 0;
}

int _lock_release_recursive(_LOCK_RECURSIVE_T *s)
{
  //RigelPrint(2);
	//RigelPrint(s);
  //Undefined results if you unlock a lock you don't own,
  //but that is already the case for many other lock implementations.
  if(((int)global_load(&(s->count))) == 1) //atomic_dec() will release the lock
    global_store(&(s->owner), (void *)-1); //null out the owner first
  atomic_dec(&(s->count));
  return 0;
}
