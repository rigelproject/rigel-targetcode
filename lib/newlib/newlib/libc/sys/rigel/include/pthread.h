#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include "rigellib.h" //For __CACHE_LINE_SIZE_BYTES

#include <sched.h> //pthread.h must implicitly include sched.h to get sched_yield() and friends.
#include <sys/cdefs.h>

typedef unsigned int pthread_t;
typedef unsigned int pthread_attr_t; //TODO: For now, this is just a dummy.  If somebody passes a non-NULL one into pthread_create(), barf.
typedef unsigned int pthread_key_t;
typedef int          pthread_once_t;

#define PTHREAD_ONCE_INIT 0

# define __SIZEOF_PTHREAD_MUTEX_T 24
# define __SIZEOF_PTHREAD_MUTEXATTR_T 4
# define __SIZEOF_PTHREAD_COND_T __CACHE_LINE_SIZE_BYTES
# define __SIZEOF_PTHREAD_CONDATTR_T 4

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

typedef struct __pthread_internal_slist
{
  struct __pthread_internal_slist *__next;
} __pthread_slist_t;

/* Data structures for mutex handling.  The structure of the attribute
   type is not exposed on purpose.  */
typedef union
{
  struct __pthread_mutex_s
  {
    int __lock;
    unsigned int __count;
    int __owner;

    int __kind;
    unsigned int __nusers;
    __extension__ union
    {
      int __spins;
      __pthread_slist_t __list;
    };
  } __data;
  char __size[__SIZEOF_PTHREAD_MUTEX_T];
  long int __align;
} pthread_mutex_t;

typedef union
{
  char __size[__SIZEOF_PTHREAD_MUTEXATTR_T];
  int __align;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER { { 0, 0, -1, 0, 0, { 0 } } }

/* Data structure for conditional variable handling.  The structure of
   the attribute type is not exposed on purpose.  */
typedef union __attribute__ ((aligned (__CACHE_LINE_SIZE_BYTES) ))
{
  struct __pthread_cond_s
  {
    int __broadcast;
    void *__mutex;
  } __data;
  char __size[__SIZEOF_PTHREAD_COND_T];
} pthread_cond_t;

typedef union
{
  char __size[__SIZEOF_PTHREAD_CONDATTR_T];
  int __align;
} pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER { { 0, (void *)0 } }

__BEGIN_DECLS

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);
int pthread_join(pthread_t thread, void **retval);

int pthread_equal(pthread_t t1, pthread_t t2);

//We need these for libc++:
//1) pthread_self(), pthread_create(), pthread_join(), pthread_detach()
//2) pthread_mutex_t -- pthread_mutex{_lock,_unlock,_trylock,attr_set,attr_destroy}()
//3) pthread_cond_t -- pthread_cond_{wait, timedwait, signal, broadcast, destroy}()
//4) pthread_key_t -- pthread_key_{create,delete}(), pthread_setspecific()

void *pthread_getspecific(pthread_key_t);
int   pthread_key_create(pthread_key_t *, void (*)(void *));
int   pthread_key_delete(pthread_key_t);
int   pthread_setspecific(pthread_key_t, const void *);

int   pthread_once(pthread_once_t *, void (*)(void));

int   pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int   pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int   pthread_cond_signal(pthread_cond_t *);
int   pthread_cond_broadcast(pthread_cond_t *);
int   pthread_cond_destroy(pthread_cond_t *);

int   pthread_mutex_trylock(pthread_mutex_t *);
int   pthread_mutex_unlock(pthread_mutex_t *);
int   pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int   pthread_mutex_lock(pthread_mutex_t *);
int   pthread_mutexattr_destroy(pthread_mutexattr_t *);
int   pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int   pthread_mutexattr_init(pthread_mutexattr_t *);
int   pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);

/*

// int   pthread_attr_destroy(pthread_attr_t *);
// int   pthread_attr_getdetachstate(const pthread_attr_t *, int *);
// int   pthread_attr_getguardsize(const pthread_attr_t *, size_t *);
// int   pthread_attr_getinheritsched(const pthread_attr_t *, int *);
// int   pthread_attr_getschedparam(const pthread_attr_t *,
//          struct sched_param *);
// int   pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
// int   pthread_attr_getscope(const pthread_attr_t *, int *);
// int   pthread_attr_getstackaddr(const pthread_attr_t *, void **);
// int   pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
// int   pthread_attr_init(pthread_attr_t *);
// int   pthread_attr_setdetachstate(pthread_attr_t *, int);
// int   pthread_attr_setguardsize(pthread_attr_t *, size_t);
// int   pthread_attr_setinheritsched(pthread_attr_t *, int);
// int   pthread_attr_setschedparam(pthread_attr_t *,
//           const struct sched_param *);
// int   pthread_attr_setschedpolicy(pthread_attr_t *, int);
// int   pthread_attr_setscope(pthread_attr_t *, int);
// int   pthread_attr_setstackaddr(pthread_attr_t *, void *);
// int   pthread_attr_setstacksize(pthread_attr_t *, size_t);
// int   pthread_cancel(pthread_t);
// void  pthread_cleanup_push(void*), void *);
// void  pthread_cleanup_pop(int);

int   pthread_cond_timedwait(pthread_cond_t *, 
          pthread_mutex_t *, const struct timespec *);

// int   pthread_condattr_destroy(pthread_condattr_t *);
// int   pthread_condattr_getpshared(const pthread_condattr_t *, int *);
// int   pthread_condattr_init(pthread_condattr_t *);
// int   pthread_condattr_setpshared(pthread_condattr_t *, int);
int   pthread_detach(pthread_t);
// int   pthread_equal(pthread_t, pthread_t);
// void  pthread_exit(void *);
// int   pthread_getconcurrency(void);
// int   pthread_getschedparam(pthread_t, int *, struct sched_param *);
int   pthread_mutex_destroy(pthread_mutex_t *);
// int   pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);

// int   pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);


// int   pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *,
//           int *);
// int   pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
// int   pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);


// int   pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
// int   pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
// int   pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);

// int   pthread_rwlock_destroy(pthread_rwlock_t *);
// int   pthread_rwlock_init(pthread_rwlock_t *,
//           const pthread_rwlockattr_t *);
// int   pthread_rwlock_rdlock(pthread_rwlock_t *);
// int   pthread_rwlock_tryrdlock(pthread_rwlock_t *);
// int   pthread_rwlock_trywrlock(pthread_rwlock_t *);
// int   pthread_rwlock_unlock(pthread_rwlock_t *);
// int   pthread_rwlock_wrlock(pthread_rwlock_t *);
// int   pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
// int   pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *,
//           int *);
// int   pthread_rwlockattr_init(pthread_rwlockattr_t *);
// int   pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
pthread_t
      pthread_self(void);
// int   pthread_setcancelstate(int, int *);
// int   pthread_setcanceltype(int, int *);
// int   pthread_setconcurrency(int);
// int   pthread_setschedparam(pthread_t, int ,
//           const struct sched_param *);
// void  pthread_testcancel(void);

*/

__END_DECLS

#endif //#ifndef __PTHREAD_H__

