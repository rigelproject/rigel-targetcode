#include <sys/signal.h>

//HACK FIXME This is a straight hack to make libunwind (and thus libcxxrt, thus libc++) compile.
//It doesn't do anything, because currently Rigel has no notion of signals.
//We should fix that at some point.
int sigprocmask (int how,const sigset_t *set,sigset_t *oldset)
{
  return 0;
  //From Linux __sigprocmask():
  //return __rt_sigprocmask(how, set, oldset, NSIG/8);
}
