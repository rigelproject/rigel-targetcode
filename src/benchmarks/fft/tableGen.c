#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI           3.14159265358979323846f
#endif

int main(int argc, char *argv[])
{
  printf("REAL sinThetaLUT[] = {\n");
  for(int i = 0; i < 32; i++)
  {
    printf("  %.10f", sinf(M_PI/(float)(1 << i)));
    if(i != (32-1))
      printf(", ");
    printf("\n");
  }
  printf("};\n");
  printf("\n");
}
