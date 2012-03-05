#include <stdio.h>
#include "rigel.h"

const int nn = 16;

int main(int argc, char *argv[])
{
  int n, m, j, i;
  if (nn<2 || nn&(nn-1))
  {
    printf("Error: nn must be power of 2 in four1 (not %d)\n", nn);
    RigelAbort();
  }
  n = nn << 1;
  j = 1;

  for(i = 1; i < n; i += 2)
  {
    if (j > i)
    {
			printf("Swapping elements %02x and %02x\n", (i-1)/2, (j-1)/2);
    }
    m = nn;
    while(m >= 2 && j > m)
    {
      j -= m;
      m >>= 1;
    }
    j += m;
  }
  return 0;
}

