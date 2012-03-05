#include "../common/common.h"
#include "rigel.h"
#include <stdio.h>
#include <stdlib.h>
#include <spinlock.h>
#include "rigel-tasks.h"

#define MAX_SUPPORTED_THREADS 8192

#define Index3D(_nx,_ny,_i,_j,_k) ((_i)+(_nx)*((_j)+(_ny)*(_k)))

#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define MIN(x,y) (((x) > (y)) ? (y) : (x))

ATOMICFLAG_INIT_CLEAR(flag);
RigelBarrierMT_Info bar;
int incohernt_malloc_enabled;
const int QID = 0;

unsigned int nx, ny, nz, timesteps;
unsigned int xmin[MAX_SUPPORTED_THREADS], xmax[MAX_SUPPORTED_THREADS];
unsigned int ymin[MAX_SUPPORTED_THREADS], ymax[MAX_SUPPORTED_THREADS];
unsigned int zmin[MAX_SUPPORTED_THREADS], zmax[MAX_SUPPORTED_THREADS];
unsigned int nxblocks, nyblocks, nzblocks;
unsigned int nxcheck, nycheck, nzcheck;
unsigned int blockDim1, blockDim2, blockDim3;
unsigned int macroBlockSizex, macroBlockSizey, macroBlockSizez;
unsigned int microBlockSizex, microBlockSizey, microBlockSizez;

unsigned int xpos[MAX_SUPPORTED_THREADS], ypos[MAX_SUPPORTED_THREADS], zpos[MAX_SUPPORTED_THREADS]; //Can't stack-allocate these in calculateBounds() :(
unsigned int sizes[MAX_SUPPORTED_THREADS];

float *bigbuf, *Anext, *Anextorig;
float *A0, *A0orig;

int TCMM_INV_LAZY;
int TCMM_INV_EAGER;
int TCMM_WRB_LAZY;
int TCMM_WRB_EAGER;

void calculateBounds(int xDivs, int xDivSize, int yDivs, int yDivSize, int zDivs, int zDivSize, int numThreads);
void calculateBlockBounds(int size, int numThreads, unsigned int *minArray, unsigned int *maxArray);
void printBlocks();

#include "stencil_blocked.c"

int main(int argc,char *argv[])
{
  RigelPrint(RigelGetThreadNum());
  int i, j, k, l;
  int threads = RigelGetNumThreads();
  int thread = RigelGetThreadNum();
  if(thread == 0)
  {
    SIM_SLEEP_ON();

    // Set it so all can see.
    incohernt_malloc_enabled = RigelIncoherentMallocEnabled();
    int nBytes;
    char inFilename[128];

    // First parameter is invalidation strategy
    RTM_SetWritebackStrategy();

    fscanf(stdin, "%s %d %d %d", inFilename, &blockDim1, &blockDim2, &blockDim3);
    fscanf(stdin, "%d %d %d %d", &nxblocks, &nyblocks, &nzblocks, &timesteps);
    //printf("input='%s', blocking by dimensions %d,%d,%d\n", inFilename, blockDim1, blockDim2, blockDim3);
		//printf("%d x %d x %d macroblocks, %d timesteps\n", nxblocks, nyblocks, nzblocks, timesteps); 
    
    FILE *inFile;
    if((inFile = fopen(inFilename, "rb")) == NULL)
    {
      printf("Error: could not open %s\n", inFilename);
      RigelAbort();
    }
    
    //Make inFile unbuffered to speed up fread()
		setbuf(inFile, NULL);

		nBytes = fread(&nx, sizeof(unsigned int), 1, inFile);
		nBytes = fread(&ny, sizeof(unsigned int), 1, inFile);
		nBytes = fread(&nz, sizeof(unsigned int), 1, inFile);

    // allocate arrays, 32-byte-aligned
		//Option A: Make arrays adjacent
    //bigbuf = (float*)malloc(sizeof(float)*nx*ny*nz*2 + 32);
    //Anext = (float*)((unsigned int)bigbuf & 0xFFFFFFE0U);
    //A0 = Anext+(nx*ny*nz);
		//Option B: Separate malloc() calls for each array
    Anextorig=(float*)malloc(sizeof(float)*nx*ny*nz + 32);
    Anext = (float*)((((unsigned int)Anextorig)+31) & 0xFFFFFFE0U);
    RigelPrint(Anext);
    A0orig= (float*)malloc(sizeof(float)*nx*ny*nz + 32);
    A0 = (float*)((((unsigned int)A0orig)+31) & 0xFFFFFFE0U);
    RigelPrint(A0);
    // fill in A0
		nBytes = fread(A0, sizeof(float)*nx*ny*nz, 1, inFile);
    RigelPrint(1);
    if(nBytes != 1)
    {
      printf("Error: Read %d of %d elements", nBytes, 1);
      RigelAbort();
    }
    RigelPrint(2);
    //This is a really inelegant way to copy A0 to Anext, but I think it may be faster than having the target do it.
    rewind(inFile);
		nBytes = fread(&nx, sizeof(unsigned int), 1, inFile);
		nBytes = fread(&ny, sizeof(unsigned int), 1, inFile);
		nBytes = fread(&nz, sizeof(unsigned int), 1, inFile);
    // fill in Anext (only boundary conditions needed, but filling in the whole thing is easier)
		nBytes = fread(Anext, sizeof(float)*nx*ny*nz, 1, inFile);
    fclose(inFile);
    if((((nx-2) % nxblocks) != 0) || (((ny-2) % nyblocks) != 0) || (((nz-2) % nzblocks) != 0))
    {
      printf("Error: Matrix does not divide evenly into n{xyz}blocks\n");
			printf("nx=%d, ny=%d, nz=%d\n", nx, ny, nz);
			printf("nxblocks=%d, nyblocks=%d, nzblocks=%d\n", nxblocks, nyblocks, nzblocks);
      RigelAbort();
    }

    macroBlockSizex = (nx-2)/nxblocks;
    macroBlockSizey = (ny-2)/nyblocks;
    macroBlockSizez = (nz-2)/nzblocks;

    microBlockSizex = macroBlockSizex;
    microBlockSizey = macroBlockSizey;
    microBlockSizez = macroBlockSizez;

    if(blockDim1 == 0) //x first
    {
      if(macroBlockSizex >= threads)
      {
        if(macroBlockSizex % threads != 0)
        {
          printf("Error: macroBlockSizex %% threads != 0\n");
          RigelAbort();
        }
        microBlockSizex = macroBlockSizex/threads;
      }
      else
      {
        microBlockSizex = 1;
        if(blockDim2 == 1) //y second
        {
          unsigned int numDivsLeft = threads/macroBlockSizex;
          if(macroBlockSizey % numDivsLeft != 0)
          {
            printf("Error: y dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizey >= numDivsLeft)
          {
            microBlockSizey = macroBlockSizey / numDivsLeft;
          }
          else
          {
            microBlockSizey = 1;
            numDivsLeft /= macroBlockSizey;
            if(macroBlockSizez < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizez % numDivsLeft != 0)
            {
              printf("Error: z dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizez = macroBlockSizez / numDivsLeft;
          }
        }
        else //z second
        {
          unsigned int numDivsLeft = threads/macroBlockSizex;
          if(macroBlockSizez % numDivsLeft != 0)
          {
            printf("Error: z dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizez >= numDivsLeft)
          {
            microBlockSizez = macroBlockSizez / numDivsLeft;
          }
          else
          {
            microBlockSizez = 1;
            numDivsLeft /= macroBlockSizez;
            if(macroBlockSizey < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizey % numDivsLeft != 0)
            {
              printf("Error: y dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizey = macroBlockSizey / numDivsLeft;
          }
        }
      }
    }
    else if(blockDim1 == 1) //y first
    {
      if(macroBlockSizey >= threads)
      {
        if(macroBlockSizey % threads != 0)
        {
          printf("Error: macroBlockSizey %% threads != 0\n");
          RigelAbort();
        }
        microBlockSizey = macroBlockSizey/threads;
      }
      else
      {
        microBlockSizey = 1;
        if(blockDim2 == 0) //x second
        {
          unsigned int numDivsLeft = threads/macroBlockSizey;
          if(macroBlockSizex % numDivsLeft != 0)
          {
            printf("Error: x dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizex >= numDivsLeft)
          {
            microBlockSizex = macroBlockSizex / numDivsLeft;
          }
          else
          {
            microBlockSizex = 1;
            numDivsLeft /= macroBlockSizex;
            if(macroBlockSizez < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizez % numDivsLeft != 0)
            {
              printf("Error: z dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizez = macroBlockSizez / numDivsLeft;
          }
        }
        else //z second
        {
          unsigned int numDivsLeft = threads/macroBlockSizey;
          if(macroBlockSizez % numDivsLeft != 0)
          {
            printf("Error: z dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizez >= numDivsLeft)
          {
            microBlockSizez = macroBlockSizez / numDivsLeft;
          }
          else
          {
            microBlockSizez = 1;
            numDivsLeft /= macroBlockSizez;
            if(macroBlockSizex < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizex % numDivsLeft != 0)
            {
              printf("Error: x dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizex = macroBlockSizex / numDivsLeft;
          }
        }
      }
    }
    else if(blockDim1 == 2) //z first
    {
      if(macroBlockSizez >= threads)
      {
        if(macroBlockSizez % threads != 0)
        {
          printf("Error: macroBlockSizez %% threads != 0\n");
          RigelAbort();
        }
        microBlockSizez = macroBlockSizez/threads;
      }
      else
      {
        microBlockSizez = 1;
        if(blockDim2 == 0) //x second
        {
          unsigned int numDivsLeft = threads/macroBlockSizez;
          if(macroBlockSizex % numDivsLeft != 0)
          {
            printf("Error: x dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizex >= numDivsLeft)
          {
            microBlockSizex = macroBlockSizex / numDivsLeft;
          }
          else
          {
            microBlockSizex = 1;
            numDivsLeft /= macroBlockSizex;
            if(macroBlockSizey < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizey % numDivsLeft != 0)
            {
              printf("Error: y dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizey = macroBlockSizey / numDivsLeft;
          }
        }
        else //y second
        {
          unsigned int numDivsLeft = threads/macroBlockSizez;
          if(macroBlockSizey % numDivsLeft != 0)
          {
            printf("Error: y dimension does not divide evenly\n");
            RigelAbort();
          }
          if(macroBlockSizey >= numDivsLeft)
          {
            microBlockSizey = macroBlockSizey / numDivsLeft;
          }
          else
          {
            microBlockSizey = 1;
            numDivsLeft /= macroBlockSizey;
            if(macroBlockSizex < numDivsLeft)
            {
              printf("Error: Matrix is too small\n");
              RigelAbort();
            }
            if(macroBlockSizex % numDivsLeft != 0)
            {
              printf("Error: x dimension does not divide evenly\n");
              RigelAbort();
            }
            microBlockSizex = macroBlockSizex / numDivsLeft;
          }
        }
      }
    }
    else //not 0, 1, or 2 (bad)
    {
      printf("Error: blockSize1 must be 0, 1, or 2\n");
      RigelAbort();
    }

    printf("%dx%dx%d\n", nx, ny, nz);
    printf("%dx%dx%d macroblocks\n", macroBlockSizex, macroBlockSizey, macroBlockSizez);
    printf("%dx%dx%d microblocks, %d ts\n", microBlockSizex, microBlockSizey, microBlockSizez, timesteps);
    unsigned int product = (macroBlockSizex/microBlockSizex)*(macroBlockSizey/microBlockSizey)*(macroBlockSizez/microBlockSizez);
    if(product != threads)
    {
      printf("Product (%d) != Threads (%d)\n", product, threads);
      RigelAbort();
    }

    calculateBounds(macroBlockSizex/microBlockSizex, microBlockSizex, macroBlockSizey/microBlockSizey, microBlockSizey, macroBlockSizez/microBlockSizez, microBlockSizez, threads);

    //printBlocks();
    
    // initialize arrays to all ones
#ifdef CIRCULARQUEUEPROBE
    if (timesteps > 1) {
      CircularQueueInit(nx, ty, timesteps);
    }
#endif
    RigelBarrierMT_Init(&bar);

    if(threads > MAX_SUPPORTED_THREADS)
    {
      printf("Error: %d threads in system, only support %d\n", threads, MAX_SUPPORTED_THREADS);
      RigelAbort();
    }

    RigelFlushLine(&macroBlockSizex);
    RigelFlushLine(&macroBlockSizey);
    RigelFlushLine(&macroBlockSizez);
    RigelFlushLine(&microBlockSizex);
    RigelFlushLine(&microBlockSizey);
    RigelFlushLine(&microBlockSizez);
    RigelFlushLine(&timesteps);
    RigelFlushLine(&A0);
    RigelFlushLine(&Anext);
    RigelFlushLine(&nxblocks);
    RigelFlushLine(&nyblocks);
    RigelFlushLine(&nzblocks);

   // printBlocks();
    
    printf("start\n");
    ClearTimer(0);
    SIM_SLEEP_OFF();
    StartTimer(0);
    atomic_flag_set(&flag);
  }
  else
  {
    atomic_flag_spin_until_set(&flag);
  }

  int xfloor, yfloor, zfloor;
  int xminl = xmin[thread];
  int xmaxl = xmax[thread];
  int yminl = ymin[thread];
  int ymaxl = ymax[thread];
  int zminl = zmin[thread];
  int zmaxl = zmax[thread];
  int xa, xb, ya, yb, za, zb;

  for(i = 0; i < timesteps; i += 2)
  {
    for(l = 0; l < nzblocks; l++)
    {
      zfloor = l*macroBlockSizez+1;
      za = zfloor+zminl;
      zb = zfloor+zmaxl;
      for(k = 0; k < nyblocks; k++)
      {
        yfloor = k*macroBlockSizey+1;
        ya = yfloor+yminl;
        yb = yfloor+ymaxl;
        for(j = 0; j < nxblocks; j++)
        {
          xfloor = j*macroBlockSizex+1;
          xa = xfloor+xminl;
          xb = xfloor+xmaxl;
					//Print this to track run progress
	        RigelPrint(0xFF000000 | (xa << 16) | (ya << 8) | (za));
          stencil_1ts(A0, Anext, xa, xb, ya, yb, za, zb);
          if(TCMM_WRB_EAGER && !incohernt_malloc_enabled)
            stencil_1ts_writeback(A0, Anext, xa, xb, ya, yb, za, zb);
          if(TCMM_INV_EAGER && !incohernt_malloc_enabled)
            stencil_1ts_invalidate(A0, Anext, xa, xb, ya, yb, za, zb);
        }
      }
    }

    if (!incohernt_malloc_enabled)
    {
      if(TCMM_WRB_LAZY || TCMM_INV_LAZY)
      {
        for(l = 0; l < nzblocks; l++)
        {
          zfloor = l*macroBlockSizez+1;
          za = zfloor+zminl;
          zb = zfloor+zmaxl;
          for(k = 0; k < nyblocks; k++)
          {
            yfloor = k*macroBlockSizey+1;
            ya = yfloor+yminl;
            yb = yfloor+ymaxl;
            for(j = 0; j < nxblocks; j++)
            {
              xfloor = j*macroBlockSizex+1;
              xa = xfloor+xminl;
              xb = xfloor+xmaxl;
              if(TCMM_WRB_LAZY)
                stencil_1ts_writeback(A0, Anext, xa, xb, ya, yb, za, zb);
              if(TCMM_INV_LAZY)
                stencil_1ts_invalidate(A0, Anext, xa, xb, ya, yb, za, zb);
            }
          }
        }
      }
    }

    RigelBarrierMT_EnterFull(&bar);
    if(i + 1 == timesteps)
      break;

    for(l = 0; l < nzblocks; l++)
    {
      zfloor = l*macroBlockSizez+1;
      za = zfloor+zminl;
      zb = zfloor+zmaxl;
      for(k = 0; k < nyblocks; k++)
      {
        yfloor = k*macroBlockSizey+1;
        ya = yfloor+yminl;
        yb = yfloor+ymaxl;
        for(j = 0; j < nxblocks; j++)
        {
          xfloor = j*macroBlockSizex+1;
          xa = xfloor+xminl;
          xb = xfloor+xmaxl;
          stencil_1ts(Anext, A0, xa, xb, ya, yb, za, zb);
          if(TCMM_WRB_EAGER && !incohernt_malloc_enabled)
            stencil_1ts_writeback(Anext, A0, xa, xb, ya, yb, za, zb);
          if(TCMM_INV_EAGER && !incohernt_malloc_enabled)
            stencil_1ts_invalidate(Anext, A0, xa, xb, ya, yb, za, zb);
        }
      }
    }

    if (!incohernt_malloc_enabled)
    {
      if(TCMM_WRB_LAZY || TCMM_INV_LAZY)
      {
        for(l = 0; l < nzblocks; l++)
        {
          zfloor = l*macroBlockSizez+1;
          za = zfloor+zminl;
          zb = zfloor+zmaxl;
          for(k = 0; k < nyblocks; k++)
          {
            yfloor = k*macroBlockSizey+1;
            ya = yfloor+yminl;
            yb = yfloor+ymaxl;
            for(j = 0; j < nxblocks; j++)
            {
              xfloor = j*macroBlockSizex+1;
              xa = xfloor+xminl;
              xb = xfloor+xmaxl;
              if(TCMM_WRB_LAZY)
                stencil_1ts_writeback(Anext, A0, xa, xb, ya, yb, za, zb);
              if(TCMM_INV_LAZY)
                stencil_1ts_invalidate(Anext, A0, xa, xb, ya, yb, za, zb);
            }
          }
        }
      }
    }
    RigelBarrierMT_EnterFull(&bar);
  }

  if(thread == 0)
  {
    StopTimer(0);
    //fr  ee(Anextorig);
    //free(A0orig);
  }

  return EXIT_SUCCESS;
}

void calculateBounds(int xDivs, int xDivSize, int yDivs, int yDivSize, int zDivs, int zDivSize, int numThreads)
{
  //printf("xDivs: %d, xDivSize: %d\n", xDivs, xDivSize);
  //printf("yDivs: %d, yDivSize: %d\n", yDivs, yDivSize);
  //printf("zDivs: %d, zDivSize: %d\n", zDivs, zDivSize);
  int i;
  int x = 0, y = 0, z = 0;
  for(i = 0; i < numThreads; i++)
  {
    //xpos[i] = i % xDivs;
    //ypos[i] = (i/xDivs) % yDivs;
    //zpos[i] = (i/(xDivs)) / yDivs;
    
    
    //printf("T%d: %d", i, x);
    //printf(" %d %d\n", y, z);
    xpos[i] = x;
    ypos[i] = y;
    zpos[i] = z;
    x++;
    if(x >= xDivs)
    {
      x = 0;
      y++;
      if(y >= yDivs)
      {
        y = 0;
        z++;
      }
    }
    
  }
  //printf("zDivSize: %d, zpos[0]: %d\n", zDivSize, zpos[0]);
  
  for(i = 0; i < numThreads; i++)
  {
    xmin[i] = xpos[i]*xDivSize;
    xmax[i] = (xpos[i]+1)*xDivSize;
    ymin[i] = ypos[i]*yDivSize;
    ymax[i] = (ypos[i]+1)*yDivSize;
    zmin[i] = zpos[i]*zDivSize;
    zmax[i] = (zpos[i]+1)*zDivSize;
    //printf("Thread %d: %d - %d, ", i, xmin[i], xmax[i]);
    //printf("%d - %d, ", ymin[i], ymax[i]);
    //printf("%d - %d\n", zmin[i], zmax[i]);
  }
  
  
  for(i = 0; i < numThreads; i += 8)
  {
    RigelFlushLine(&(xmin[i]));
    RigelFlushLine(&(xmax[i]));
    RigelFlushLine(&(ymin[i]));
    RigelFlushLine(&(ymax[i]));
    RigelFlushLine(&(zmin[i]));
    RigelFlushLine(&(zmax[i]));
  }
  
}

void calculateBlockBounds(int size, int numThreads, unsigned int *minArray, unsigned int *maxArray)
{
  unsigned int i;
  int remaining = size - 2;
  int minimum = remaining/numThreads;
  for(i = 0; i < numThreads; i++)
    sizes[i] = minimum;
  remaining -= (minimum*numThreads);
  int idx = 0;
  while(remaining > 0)
  {
    sizes[idx]++;
    idx++;
    remaining--;
  }
  int counter = 1; //Only do 1..{nx,ny,nz}-1 b/c of boundary conditions
  for(i = 0; i < numThreads; i++)
  {
    minArray[i] = counter;
    counter += sizes[i];
    maxArray[i] = counter;
  }
  for(i = 0; i < numThreads; i += 8)
  {
      RigelFlushLine(&(minArray[i]));
      RigelFlushLine(&(maxArray[i]));
  }
}

void printBlocks()
{
  int i;
  int threads = RigelGetNumThreads();
  printf("%dx%dx%d matrix\n", nx, ny, nz);
  for(i = 0; i < threads; i++)
  {
    printf("Thread %d: ", i);
    printf("%d-%d, ", xmin[i], xmax[i]);
    printf("%d-%d, ", ymin[i], ymax[i]);
    printf("%d-%d\n", zmin[i], zmax[i]);
  }
  for(i = 0; i < threads; i++)
  {
    RigelPrint(i);
    RigelPrint((xmin[i] << 16) | xmax[i]);
    RigelPrint((ymin[i] << 16) | ymax[i]);
    RigelPrint((zmin[i] << 16) | zmax[i]);
  }
}
