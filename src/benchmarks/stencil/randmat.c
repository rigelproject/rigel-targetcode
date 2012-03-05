#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

int main(int argc, char *argv[])
{
  uint32_t nx, ny, nz;
  float minVal, maxVal;
  FILE *outFile;

  if(argc != 7)
  {
    printf("Usage: %s nx ny nz minVal maxVal outputFilename\n", argv[0]);
    exit(1);
  }

  if(sscanf(argv[1], "%"SCNu32"", &nx) != 1)
  {
    printf("Error: nx value '%s' is not a valid unsigned int\n", argv[1]);
    exit(1);
  }
  if(sscanf(argv[2], "%"SCNu32"", &ny) != 1)
  {
    printf("Error: ny value '%s' is not a valid unsigned int\n", argv[2]);
    exit(1);
  }
  if(sscanf(argv[3], "%"SCNu32"", &nz) != 1)
  {
    printf("Error: nz value '%s' is not a valid unsigned int\n", argv[3]);
    exit(1);
  }
  if(sscanf(argv[4], "%f", &minVal) != 1)
  {
    printf("Error: minVal value '%s' is not a valid float\n", argv[4]);
    exit(1);
  }
  if(sscanf(argv[5], "%f", &minVal) != 1)
  {
    printf("Error: maxVal value '%s' is not a valid float\n", argv[5]);
    exit(1);
  }
  if((outFile = fopen(argv[6], "wb")) == NULL)
  {
    printf("Error: could not open %s\n", argv[6]);
    exit(1);
  }

  fwrite(&nx, sizeof(nx), (size_t)1, outFile);
  fwrite(&ny, sizeof(ny), (size_t)1, outFile);
  fwrite(&nz, sizeof(nz), (size_t)1, outFile);

  float randFloat;

  for(uint64_t i = 0; i < (uint64_t)nx*ny*nz; i++)
  {
    randFloat = ((float)rand() / (float)RAND_MAX)*((float)maxVal-(float)minVal)+(float)minVal;
    fwrite(&randFloat, sizeof(randFloat), (size_t)1, outFile);
  }

  fclose(outFile);
  return 0;
}

