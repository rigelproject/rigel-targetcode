/* Rigel system call emulation code
   Copyright (C) 2011 Matthew R. Johnson */

#include <machine/suds_primops.h>
#include <sys/stat.h>
#include "../syscall.h"
#include <rigel.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>

int
_read (file, ptr, len)
     int    file;
     char * ptr;
     int    len;
{
  //RigelPrint(0xAAAA0000);
  return (int)(__suds_syscall(0x82,
                              (__uint32_t)file,
                              (__uint32_t)ptr,
                              (__uint32_t)len));
}

int
_lseek (file, ptr, dir)
     int file;
     int ptr;
     int dir;
{
  //RigelPrint(0xAAAA0001);
  return (int)(__suds_syscall(0x84,
                              (__uint32_t)file,
                              (__uint32_t)ptr,
                              (__uint32_t)dir));
}

int
_write (file, ptr, len)
     int    file;
     char * ptr;
     int    len;
{
  //RigelPrint(0xAAAA0002);
  return (int)(__suds_syscall(0x83,
                              (__uint32_t)file,
                              (__uint32_t)ptr,
                              (__uint32_t)len));
}

int
_open (path, flags, mode)
     const char * path;
     int flags;
		 int mode;
{
  //RigelPrint(0xAAAA0003);
  return (int)(__suds_syscall(0x80,
                              (__uint32_t)path,
                              (__uint32_t)flags,
                              (__uint32_t)mode));
}

int
_close (file)
     int file;
{
  //RigelPrint(0xAAAA0004);
  return (int)(__suds_syscall(0x81, (__uint32_t)file, 0, 0));
}

void
_exit (n)
     int n;
{
  //RigelPrint(0xAAAA0005);
	if(n == 0) //Exit cleanly
    __suds_halt();
	else //Something is wrong, tell the simulator to stop
		RigelAbort();
}


caddr_t 
_sbrk (incr)
     int incr;
{
  //RigelPrint(0xAAAA0006);
/*
  extern char   end asm ("_end");	// Defined by the linker
  extern int    __stack;          //Defined by linker script.
  static char * heap_end;
  char *        prev_heap_end;

  if (heap_end == NULL)
    heap_end = & end;
  
  prev_heap_end = heap_end;
#if 0  
  if (heap_end + incr > __stack)
    {
      _write ( 1, "_sbrk: Heap and stack collision\n", 32);
      abort ();
    }
#endif
  heap_end += incr;

  return (caddr_t) prev_heap_end;
*/
	extern char  _end; /* Defined by the linker script */
  static char *heap_end;
  char *       prev_heap_end;

  if (heap_end == NULL)
    heap_end = &_end;
  
  prev_heap_end = heap_end;
  heap_end += incr;

  return (caddr_t) prev_heap_end;
}

int
_fstat (file, st)
     int file;
     struct stat * st;
{
  //RigelPrint(0xAAAA0007);
  st->st_mode = S_IFCHR;
  return 0;
}

int
_stat (filename, st)
	     const char *filename;
			 struct stat * st;
{
	  //RigelPrint(0xAAAA0008);
		  st->st_mode = S_IFCHR;
			  return 0;
}

int
_unlink ()
{
  //RigelPrint(0xAAAA0009);
  return -1;
}

int
_isatty (fd)
     int fd;
{
  //RigelPrint(0xAAAA000A);
  return 0;
}

int
_raise ()
{
  //RigelPrint(0xAAAA000B);
  return 0;
}

clock_t
_times (struct tms *buf)
{
  //RigelPrint(0xAAAA000C);
	//errno = EINVAL;
  return -1;
}

int
_kill (pid, sig)
     int pid;
     int sig;
{
  //RigelPrint(0xAAAA000D);
	RigelAbort();
  return 0;
}

int
_getpid (void)
{
  //RigelPrint(0xAAAA000E);
  return 0;
}

time_t _time (time_t *tod)
{
	//RigelPrint(0xAAAA000F);
  time_t t = 0xDECAFBAD;
  return t;
}

/* _gettimeofday -- implement in terms of time.  */
int _gettimeofday (struct timeval *tv, void *tzvp)
{
	//RigelPrint(0xAAAA0010);
  struct timezone *tz = tzvp;
  if (tz)
    tz->tz_minuteswest = tz->tz_dsttime = 0;
	
	tv->tv_usec = 0;
	tv->tv_sec = _time (0);
	return 0;
}

