#ifndef __SUDS_ASM_H
#define __SUDS_ASM_H

#include <sys/types.h>

/****************************************************************************
 * gcc extensions to enable access to the processor's non-MIPS features.
 ***************************************************************************/

#if defined(__GNUC__)

/* stop the processor */
//static void __suds_halt() __attribute__ ((__noreturn__));
//static inline
//void
#define __suds_halt() do { __asm__ ("hlt"); } while(0)
/* processor stops here, but we need to call exit so gcc will know
     that this function dies without returning */
  //exit(1);

struct __suds_syscall_struct {
  __uint32_t syscall_num;
  __uint32_t result;
  __uint32_t arg1;
  __uint32_t arg2;
  __uint32_t arg3;
};

static inline
__uint32_t
__suds_syscall(__uint32_t syscall_num, __uint32_t arg1, __uint32_t arg2, __uint32_t arg3)
{
  struct __suds_syscall_struct sdata;
  __uint32_t sdata_addr = (__uint32_t)(&sdata);
  sdata.syscall_num = syscall_num;
  sdata.arg1 = arg1;
  sdata.arg2 = arg2;
  sdata.arg3 = arg3;

  
  __asm__ __volatile__ ("syscall %0"
                    : /* no output */
                    : "r" (sdata_addr));

  return sdata.result;
}

static inline
float
__suds_strtof(const char* str)
{
  float result;
  __asm__ ("strtof %0,%1"
           : "=f" (result)
           : "r" (str));
  return result;
}

static inline
void
__suds_syscall_fast(const struct __suds_syscall_struct *p)
{
  __asm__ __volatile__ ("syscall %0"
                      : /* no output */
                      : "r" (p));
  /* Return value is in p->result */
}

#else /* not gnu c */

/* stop the processor */
//void __suds_halt();

struct __suds_syscall_struct {
  __uint32_t syscall_num;
  __uint32_t result;
  __uint32_t arg1;
  __uint32_t arg2;
  __uint32_t arg3;
};

__uint32_t __suds_syscall(__uint32_t syscall_num,
                          __uint32_t arg1,
                          __uint32_t arg2,
                          __uint32_t arg3);

float __suds_strtof(const char* str);

#endif /* gnu c */


#endif /* __SUDS_ASM_H */
