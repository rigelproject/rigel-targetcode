#ifndef LLVM28
#error Need LLVM 2.8 to support weak functions for libpthread
#endif

#include <pthread.h>
#include <stdio.h>
#include "rigel.h"

void * test(void *arg) {
  RigelPrint(arg);
  RigelPrint(RigelGetThreadNum());
  return (void *)0x2;
}

int main(int argc, char *argv[]) {
  SIM_SLEEP_ON();
  SIM_SLEEP_OFF();
  pthread_t tidholder;
  printf("return %d\n", pthread_create(&tidholder, NULL, test, NULL));
  printf("TID %u\n", tidholder);
  void *ret;
  pthread_join(tidholder, &ret);
  printf("Returned %u\n", (unsigned int)ret);
  return 0;
}
