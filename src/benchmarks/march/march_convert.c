#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
  unsigned int obj = 0;
  char *outFilename = argv[argc-1];
  FILE *out = fopen(outFilename, "wb");
  if(out == NULL)
  {
    printf("Error: Could not open output file %s\n",outFilename);
    exit(1);
  }
  unsigned int hist[65536];
  for(int i = 0; i < 65536; i++)
    hist[i] = 0;
  unsigned int nx, ny, nz;
  sscanf(argv[1], "%u", &nx);
  sscanf(argv[2], "%u", &ny);
  nz = argc-2-2;
  printf("%u %u %u\n", nx, ny, nz);

  fwrite(&nx,sizeof(unsigned int),(size_t)1,out);
  fwrite(&ny,sizeof(unsigned int),(size_t)1,out);
  fwrite(&nz,sizeof(unsigned int),(size_t)1,out);

  for(int i = 3; i < (argc-1); i++)
  {
    FILE *in = fopen(argv[i], "rb");
    if(in == NULL)
    {
      printf("Error: Could not open input file %s\n", argv[i]);
      exit(1);
    }
    for(int j = 0; j < nx; j++)
    {
      for(int k = 0; k < ny; k++)
      {
        //Need to byte-swap, data files are big-endian.
        unsigned char byte1, byte2;
        fread(&byte1,(size_t)1,(size_t)1,in);
        fread(&byte2,(size_t)1,(size_t)1,in);
        unsigned short sDatum = (byte1 << 8) | byte2;
        hist[sDatum]++;
        float fDatum = (float)sDatum;
        fwrite(&fDatum,sizeof(fDatum),(size_t)1,out);
      }
    }
    fclose(in);
  }
  fclose(out);

  for(int i = 0; i < 65536; i++)
    if(hist[i] != 0)
      printf("%d: %u values\n", i, hist[i]);

  return 0;
}

