#define PARAM_NUMK_ELEMS 2048
#define PARAM_NUMX_ELEMS 512

/* 
computeFH.cu

CUDA code for creating the FH  data structure for fast convolution-based 
Hessian multiplication for arbitrary k-space trajectories.

Inputs:
kx - VECTOR of kx values, same length as ky and kz
ky - VECTOR of ky values, same length as kx and kz
kz - VECTOR of kz values, same length as kx and ky
x  - VECTOR of x values, same length as y and z
y  - VECTOR of y values, same length as x and z
z  - VECTOR of z values, same length as x and y
phi - VECTOR of the Fourier transform of the spatial basis 
      function, evaluated at [kx, ky, kz].  Same length as kx, ky, and kz.
*/

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mri.h"
#include "rigel.h"
#include "rigel-tasks.h"

ATOMICFLAG_INIT_CLEAR(Init_Flag);

#define PARAM_TASK_GROUP_PREFETCH 1

// Turn on for debugging
#undef DEBUG_PRINT_OUTPUT
#define DEBUG_TIMERS_ENABLED 0

#ifdef HW_TQ
  #define RIGEL_DEQUEUE TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE TaskQueue_Enqueue
  #define RIGEL_LOOP    TaskQueue_Loop  
  #define RIGEL_INIT    TaskQueue_Init
  #define RIGEL_END     TaskQueue_End
#endif

#ifdef CLUSTER_LOCAL_TQ
  #define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_CLUSTER_FAST
  #define RIGEL_ENQUEUE SW_TaskQueue_Enqueue_CLUSTER
  #define RIGEL_LOOP    SW_TaskQueue_Loop_CLUSTER_FAST
  #define RIGEL_INIT    SW_TaskQueue_Init_CLUSTER
  #define RIGEL_END     SW_TaskQueue_End_CLUSTER
#endif

#ifdef SWCC_TQ
  #define SW_COHERENCE
  #define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_CLUSTER_FAST
  #define RIGEL_ENQUEUE SW_TaskQueue_Enqueue_CLUSTER
  #define RIGEL_LOOP    SW_TaskQueue_Loop_CLUSTER_FAST
  #define RIGEL_INIT    SW_TaskQueue_Init_CLUSTER
  #define RIGEL_END     SW_TaskQueue_End_CLUSTER
#endif

#ifdef X86_RTM
  #define RIGEL_DEQUEUE	TaskQueue_Dequeue
  #define RIGEL_ENQUEUE	TaskQueue_Enqueue
  #define RIGEL_LOOP		TaskQueue_Loop	
  #define RIGEL_INIT		TaskQueue_Init
  #define RIGEL_END			TaskQueue_End
#endif

inline
void 
ComputeRhoPhi(int numK,
              volatile FPTYPE* phiR, volatile FPTYPE* phiI,
              volatile FPTYPE* dR, volatile FPTYPE* dI,
              volatile FPTYPE* realRhoPhi, volatile FPTYPE* imagRhoPhi) {
  int indexK = 0;
  for (indexK = 0; indexK < numK; indexK++) {
    realRhoPhi[indexK] = phiR[indexK] * dR[indexK] + phiI[indexK] * dI[indexK];
    imagRhoPhi[indexK] = phiR[indexK] * dI[indexK] - phiI[indexK] * dR[indexK];
  }
}

inline
void
ComputeFH(int numK, int numX,
          volatile kValues* kVals,
          FPTYPE* x, FPTYPE* y, FPTYPE* z, 
          FPTYPE *outR, FPTYPE *outI) {
  FPTYPE expArg;
  FPTYPE cosArg;
  FPTYPE sinArg;
  int kTile;
  int indexX;
  int indexK;

  int kTiles = numK / K_ELEMS_PER_GRID;
  if (numK % K_ELEMS_PER_GRID)
    kTiles++;

  for (kTile = 0; kTile < kTiles; kTile++) {
RigelPrint(0xaaa0000);
    for (indexX = 0; indexX < numX; indexX++) {
      FPTYPE regOutR = outR[indexX];
      FPTYPE regOutI = outI[indexX];
      FPTYPE regX = x[indexX];
      FPTYPE regY = y[indexX];
      FPTYPE regZ = z[indexX];

      int kTileBase = kTile * K_ELEMS_PER_GRID;
      int numElems = MIN(K_ELEMS_PER_GRID, numK - kTileBase);
      for (indexK = kTileBase; indexK < kTileBase+numElems; indexK++) {
        expArg = (kVals[indexK].Kx * regX + 
                  kVals[indexK].Ky * regY + 
                  kVals[indexK].Kz * regZ);

        cosArg = SCALAR_COS(expArg);
        sinArg = SCALAR_SIN(expArg);

        FPTYPE rRhoPhi = kVals[indexK].RealRhoPhi;
        FPTYPE iRhoPhi = kVals[indexK].ImagRhoPhi;
        regOutR += rRhoPhi * cosArg - iRhoPhi * sinArg;
        regOutI += iRhoPhi * cosArg + rRhoPhi * sinArg;
      }

      outR[indexX] = regOutR;
      outI[indexX] = regOutI;
#ifndef SW_COHERENCE
      // Only flush at the end of a cache line
      if (indexX % 8 == 7) {
        RigelWritebackLine(&outR[indexX]);
        RigelWritebackLine(&outI[indexX]);
      }
#endif
    }
  }
}

#define memalign(x, y) malloc(y)

void inputData(char* fName, volatile int* numK, volatile int* numX,
               volatile FPTYPE** kx, volatile FPTYPE** ky, volatile FPTYPE** kz,
               volatile FPTYPE** x, volatile FPTYPE** y, volatile FPTYPE** z,
               volatile FPTYPE** phiR, volatile FPTYPE** phiI,
               volatile FPTYPE** dR, volatile FPTYPE** dI) {
//  FILE* fid = fopen(fName, "r");
//  fread (numK, sizeof (int), 1, fid);
//  fread (numX, sizeof (int), 1, fid);
  *numK = PARAM_NUMK_ELEMS;
  *numX = PARAM_NUMX_ELEMS;
  *kx = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *ky = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *kz = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *x = (FPTYPE *) memalign(16, *numX * sizeof (FPTYPE));
  *y = (FPTYPE *) memalign(16, *numX * sizeof (FPTYPE));
  *z = (FPTYPE *) memalign(16, *numX * sizeof (FPTYPE));
  *phiR = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *phiI = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *dR = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
  *dI = (FPTYPE *) memalign(16, *numK * sizeof (FPTYPE));
//  fread (*kx, sizeof (FPTYPE), *numK, fid);
//  fread (*ky, sizeof (FPTYPE), *numK, fid);
//  fread (*kz, sizeof (FPTYPE), *numK, fid);
//  fread (*x, sizeof (FPTYPE), *numX, fid);
//  fread (*y, sizeof (FPTYPE), *numX, fid);
//  fread (*z, sizeof (FPTYPE), *numX, fid);
//  fread (*phiR, sizeof (FPTYPE), *numK, fid);
//  fread (*phiI, sizeof (FPTYPE), *numK, fid);
//  fread (*dR, sizeof (FPTYPE), *numK, fid);
//  fread (*dI, sizeof (FPTYPE), *numK, fid);
// fclose (fid); 
}

void createDataStructs(int numK, int numX, 
                       volatile FPTYPE** realRhoPhi, volatile FPTYPE** imagRhoPhi, 
                       volatile FPTYPE** outR, volatile FPTYPE** outI) {
  *realRhoPhi = (FPTYPE* ) memalign(16, numK * sizeof(FPTYPE));
  *imagRhoPhi = (FPTYPE* ) memalign(16, numK * sizeof(FPTYPE));
  *outR = (FPTYPE*) memalign(16, numX * sizeof (FPTYPE));
  *outI = (FPTYPE*) memalign(16, numX * sizeof (FPTYPE));
}

#if 0
void outputData(char* fName, FPTYPE* outR, FPTYPE* outI, int numX) {
  FILE* fid = fopen(fName, "w");
  fwrite (outR, sizeof (FPTYPE), numX, fid);
  fwrite (outI, sizeof (FPTYPE), numX, fid);
  fclose (fid);
}
#endif

volatile int numX, numK, inputK;
volatile FPTYPE *kx, *ky, *kz;
volatile FPTYPE *x, *y, *z, *phiR, *phiI, *dR, *dI;
volatile FPTYPE *realRhoPhi, *imagRhoPhi, *outI, *outR;
volatile kValues* kVals;
// Compute outR and outI on multiple CPUs
fhparams* GLOBAL_params = NULL;

int main ()
#ifdef X86_RTM
{
  // Initialize the emulation framework for the RTM
  X86_TASK_MODEL_INIT();
  // Call what will be 'main()' in the actual Rigel code
  X86_TASK_MODEL_WAIT();

  return 0;
}

// XXX: This must be defined by the user and will become main in the actual
// XXX: Rigel code that runs on RigelSim
int rigel_main()
#endif
{
  // Rigel-specific data
  int corenum = RigelGetCoreNum();
  int num_tasks = RigelGetNumCores();

  // XXX: Initialize data structures
  if (0 == corenum) {
    int k, i;
    char input_file_name[] = "mri.in";
    char output_file_name[] = "mri.out";
    /* Read in data */
    inputData(input_file_name, &numK, &numX, &kx, &ky, 
      &kz, &x, &y, &z, &phiR, &phiI, &dR, &dI);

     inputK = numK;

    #if DEBUG_PRINT
    printf("numX = %d,  numK = %d, inputK = %d\n", numX, numK, inputK);
    #endif
    numK = MIN(inputK, numK);

    /* Create CPU data structures */
    createDataStructs(numK, numX, &realRhoPhi, &imagRhoPhi, &outR, &outI);
    ComputeRhoPhi(numK, phiR, phiI, dR, dI, realRhoPhi, imagRhoPhi);

    kVals = (kValues*)calloc(numK, sizeof (kValues));
    for (k = 0; k < numK; k++) {
      kVals[k].Kx = PIx2 * kx[k];
      kVals[k].Ky = PIx2 * ky[k];
      kVals[k].Kz = PIx2 * kz[k];
      kVals[k].RealRhoPhi = realRhoPhi[k];
      kVals[k].ImagRhoPhi = imagRhoPhi[k];
    }

    // allocate and initialize array of thread parameters
    GLOBAL_params = (fhparams*)malloc(num_tasks * sizeof(fhparams));
    for (i = 0; i < num_tasks; i++) {
      GLOBAL_params[i].threadid = i;
      GLOBAL_params[i].threadcount = num_tasks;
      GLOBAL_params[i].totalNumK = numK;
      GLOBAL_params[i].threadBaseX = i*(numX / num_tasks);
      GLOBAL_params[i].threadNumX = (numX / num_tasks);
      GLOBAL_params[i].kVals = kVals;
      GLOBAL_params[i].x = x;
      GLOBAL_params[i].y = y;
      GLOBAL_params[i].z = z;
      GLOBAL_params[i].outR = outR;
      GLOBAL_params[i].outI = outI;
    }
    if (numX % num_tasks) {
      GLOBAL_params[num_tasks-1].threadNumX += (numX % num_tasks);
    }

    // Rigel-specific Initialization
    RIGEL_INIT(QID, MAX_TASKS);
    SW_TaskQueue_Set_TASK_GROUP_FETCH(PARAM_TASK_GROUP_PREFETCH);
    ClearTimer(0);
    StartTimer(0);
    atomic_flag_set(&Init_Flag);
  }
  atomic_flag_spin_until_set(&Init_Flag);

  GenerateTasks(num_tasks);

  if (0 == corenum) {
    StopTimer(0);

    /* free thread params */
    free(GLOBAL_params);

    /* Write result to file */
    //  outputData(output_file_name, outR, outI, numX);

    #if DEBUG_PRINT
    for (int i = 0; (i < numX) && (i < PRINT_MAX_INDEX); i = i + PRINT_SAMPLE_RATE) {
      printf ("i: %d, rea: %.15f, img: %.15f\n", i, outR[i], outI[i]);
    }
    #endif

    free((void *)kx);
    free((void *)ky);
    free((void *)kz);
    free((void *)x);
    free((void *)y);
    free((void *)z);
    free((void *)phiR);
    free((void *)phiI);
    free((void *)dR);
    free((void *)dI);
    free((void *)realRhoPhi);
    free((void *)imagRhoPhi);
    free((void *)kVals);
    free((void *)outR);
    free((void *)outI);

  }

   return 0;
}

void GenerateTasks(int num_tasks) {
  int corenum = RigelGetCoreNum();
  TQ_TaskDesc enq_tdesc;
  TQ_TaskDesc deq_tdesc;
  TQ_RetType retval;

  // TODO: START PARALLEL EXECUTION
  if (corenum == 0) {
    RIGEL_LOOP(QID, num_tasks, 1, &enq_tdesc);
  }

  while (1) {
    #ifdef HW_TQ
    TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
    #else
    retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
    #endif

    if (retval == TQ_RET_BLOCK) continue;
    if (retval == TQ_RET_SYNC) break;
    if (retval != TQ_RET_SUCCESS) RigelAbort();
    {
      taskFH(deq_tdesc.begin);
   }
  }
  // XXX: Wait for remaining threads to finish 
}

void* taskFH(int task_num) {
  FPTYPE *x, *y, *z, *outR, *outI;
  fhparams* params = &GLOBAL_params[task_num];

  int threadid;
  int threadBaseX;
  int threadNumX;

  threadid = params->threadid;
  threadBaseX = params->threadBaseX;
  threadNumX  = params->threadNumX;

  x    = (FPTYPE *)&(params->x[threadBaseX]);
  y    = (FPTYPE *)&(params->y[threadBaseX]);
  z    = (FPTYPE *)&(params->z[threadBaseX]);
  outR = (FPTYPE *)&(params->outR[threadBaseX]);
  outI = (FPTYPE *)&(params->outI[threadBaseX]);

  // Compute FH on the GPU
  ComputeFH(params->totalNumK, threadNumX, params->kVals, x, y, z, outR, outI);

  return 0;
}
