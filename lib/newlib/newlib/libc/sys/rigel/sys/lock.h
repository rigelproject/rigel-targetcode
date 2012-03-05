#ifndef __SYS_LOCK_H__
#define __SYS_LOCK_H__

#include <_ansi.h>

typedef struct {
  int owner;
  unsigned int count;
} _recursive_spin_lock_t;

typedef int _LOCK_T;
typedef _recursive_spin_lock_t _LOCK_RECURSIVE_T;

void _lock_init(_LOCK_T *lock);
void _lock_init_recursive(_LOCK_RECURSIVE_T *lock);
int _lock_try_acquire(_LOCK_T *lock);
int _lock_try_acquire_recursive(_LOCK_RECURSIVE_T *l);
void _lock_acquire(_LOCK_T *lock);
void _lock_acquire_recursive(_LOCK_RECURSIVE_T *s);
int _lock_release(_LOCK_T *l);
int _lock_release_recursive(_LOCK_RECURSIVE_T *s);

#define __LOCK_INIT(class,lock) class _LOCK_T lock = 0
#define __LOCK_INIT_RECURSIVE(class,lockname) class _LOCK_RECURSIVE_T lockname = { -1, 0 }
#define __lock_init(lock) _lock_init(&lock)
#define __lock_init_recursive(lock) _lock_init_recursive(&lock)
#define __lock_close(lock) (_CAST_VOID 0)
#define __lock_close_recursive(lock) (_CAST_VOID 0)
#define __lock_acquire(lock) _lock_acquire(&lock)
#define __lock_acquire_recursive(lock) _lock_acquire_recursive(&lock)
#define __lock_try_acquire(lock) _lock_try_acquire(&lock)
#define __lock_try_acquire_recursive(lock) _lock_try_acquire_recursive(&lock)
#define __lock_release(lock) _lock_release(&lock)
#define __lock_release_recursive(lock) _lock_release_recursive(&lock)

#endif /* __SYS_LOCK_H__ */

