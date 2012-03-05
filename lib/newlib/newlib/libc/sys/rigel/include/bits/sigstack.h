#ifndef __SIGSTACK_H__
#define __SIGSTACK_H__

#include <stddef.h>

typedef struct sigaltstack
  {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
  } stack_t;

#endif // __SIGSTACK_H__

