#include <machine/suds_primops.h>
#include <rigel.h>

///////////////////////////////////////////////////////////////////////////////
// TIMERS
///////////////////////////////////////////////////////////////////////////////

// starttimer
int
StartTimer(int timernum)
{
	//RigelPrint(0xfeedece4);
  return (int)(__suds_syscall(0x20,
                              (__uint32_t)timernum,0,0 ));
}

// gettimer
int
GetTimer(int timernum) 
{
	//RigelPrint(0xfeedece4);
  return (int)(__suds_syscall(0x21,
                              (__uint32_t)timernum,0,0 ));
}

// stoptimer
int
StopTimer(int timernum) 
{
	//RigelPrint(0xfeedece4);
  return (int)(__suds_syscall(0x22,
                              (__uint32_t)timernum,0,0 ));
}

// cleartimer
int
ClearTimer(int timernum) 
{
	//RigelPrint(0xfeedece4);
  return (int)(__suds_syscall(0x23,
                              (__uint32_t)timernum,0,0 ));
}

