#include "rigel.h"
#include <stdio.h>
#include <machine/suds_primops.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
  if(RigelGetThreadNum() == 0) {
    float accumf = 0;
    //This is a test of my wacky way of reusing syscall arguments across calls to eliminate the overhead
    //of pushing a duplicate struct to the stack on every syscall.
    //We fill in syscall_num, arg1/2/3, then do __suds_syscall_fast() with a pointer to our filled-in struct.
    //__suds_syscall_fast() returns void, but the result is in s.result.
    float min = 1000.0f, max = 2000.0f;
    struct __suds_syscall_struct s;
    s.syscall_num = 0x3D;
    s.arg1 = *((uint32_t *)&min);
    s.arg2 = *((uint32_t *)&max);
    s.arg3 = 0;

    ClearTimer(0); StartTimer(0);
    for(int i = 0; i < 50000; i++) {
      __suds_syscall_fast(&s);
      accumf += *((float *)&(s.result));
    }
    StopTimer(0);

    printf("Float: %f\n", accumf);

    for(int i = 0; i < 100; i++) {
      printf("RandUInt(0x90000000, 0xA0000000) = 0x%08X\n", RigelRandUInt(0x90000000U, 0xA0000000U));
    }
    SIM_SLEEP_OFF();
  }
  return 0;
}

