/* get per-thread reentrancy structure pointer */

#include <rigel.h>
#include <rigellib.h>

//Initialized in crt0
//FIXME We have to statically allocate these for now because our malloc implementation requires __getreent() to work,
//so we can't very well malloc an array of reentrancy structures :P
//FIXME We could statically allocate the one for thread 0, then dynamically malloc the rest in crt0.
//Bonus Points: change the pointer for thread 0's structure to point into that dynamically-allocated array :P
//struct _reent **__reent_array;
struct _reent *__reent_ptrs[__MAX_NUM_THREADS];
struct _reent __reent_data[__MAX_NUM_THREADS];

struct _reent *
__getreent (void)
{
  return __reent_ptrs[RigelGetThreadNum()];
}

