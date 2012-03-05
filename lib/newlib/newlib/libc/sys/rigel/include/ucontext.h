#ifndef __UCONTEXT_H__
#define __UCONTEXT_H__

#include <signal.h> //For sigset_t, stack_t
#include <stdint.h> //For uint32_t

typedef struct __mcontext_t
{
  uint32_t gregs[32];
} mcontext_t;

typedef struct __ucontext
{
  struct __ucontext *uc_link;   //pointer to the context that will be resumed
                                //when this context returns
  sigset_t          uc_sigmask; //the set of signals that are blocked when this
                                //context is active
  stack_t     uc_stack;          //the stack used by this context
  mcontext_t  uc_mcontext;       //a machine-specific representation of the saved
                                //context
} ucontext_t;

#endif //#ifndef __UCONTEXT_H__
