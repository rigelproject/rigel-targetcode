#include <stdlib.h>
#include <stdio.h>
#include <rigel.h>

int main(int argc, char *argv[])
{
  if(RigelGetThreadNum() == 0) {
    SIM_SLEEP_OFF();
  }
  void *mallocs[100];
  for(int i = 0; i < 100; i++) {
    mallocs[i] = malloc(RigelRandUInt(4, 2000)*4);
    RigelPrint(i);
  }
  for(int i = 0; i < 100; i++) {
    free(mallocs[i]);
    RigelPrint(0xFFFF0000 | i);
  }
  //SIM_SLEEP_OFF();
  //}
  return 0;
}

