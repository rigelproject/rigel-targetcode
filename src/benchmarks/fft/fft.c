#include "../common/common.h"
#include "rigel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <spinlock.h>
#include <string.h>
#include "rigel-tasks.h"
#include "../common/macros.h"

#define MIN(x,y) (((x)>(y)) ? (y) : (x))
#define MAX(x,y) (((x)>(y)) ? (x) : (y))

//Can be "fft" or "four1"
//fft decomposes the 1D FFTs into shorter 1D FFTs (length 512)
//for better cache behavior.
//four1 just does a vanilla 1D FFT on the whole row
#define FFT_FUNC fft

ATOMICFLAG_INIT_CLEAR(flag);

const int QID = 0;

int BLOCK_SIZE_I;
int BLOCK_SIZE_J;
int R;
int C;
unsigned int si, sj, sr, sc;

#define WRITEBACK_DESCRIPTOR(desc)

#define INVALIDATE_DESCRIPTOR(desc)

//This include must happen after the declaration of these 2 macros and all global data the macros touch
#include "../common/descriptor_buffer_fancy.h"

inline void writeback_descriptor(rtm_range_t *desc)
{

}

inline void invalidate_descriptor(rtm_range_t *desc)
{

}

// From common.h
int TCMM_INV_LAZY;
int TCMM_INV_EAGER;
int TCMM_WRB_LAZY;
int TCMM_WRB_EAGER;

#define REAL float
#define REALsin sinf

REAL *data;
REAL *data2;

REAL **workspace;

REAL sinThetaLUT[] = {
  -0.0000000874f, 
  1.0000000000f, 
  0.7071068287f, 
  0.3826834559f, 
  0.1950903237f, 
  0.0980171412f, 
  0.0490676761f, 
  0.0245412290f, 
  0.0122715384f, 
  0.0061358847f, 
  0.0030679568f, 
  0.0015339802f, 
  0.0007669904f, 
  0.0003834952f, 
  0.0001917476f, 
  0.0000958738f, 
  0.0000479369f, 
  0.0000239685f, 
  0.0000119842f, 
  0.0000059921f, 
  0.0000029961f, 
  0.0000014980f, 
  0.0000007490f, 
  0.0000003745f, 
  0.0000001873f, 
  0.0000000936f, 
  0.0000000468f, 
  0.0000000234f, 
  0.0000000117f, 
  0.0000000059f, 
  0.0000000029f, 
  -0.0000000015f
};

//data is a pointer to one REAL *before* the start of data,
//because the code assumes 1-based indices (ugh).
void four1(REAL *data, const int nn, const int isign)
{
  int n, mmax, m, j, istep, i, tblIndex;
  REAL wtemp, wr, wpr, wpi, wi, theta, tempr, tempi;
	// Calling a varargs function causes an extra 32 bytes of stack alloc
#if 0
  if (nn<2 || nn&(nn-1))
  {
    printf("Error: nn must be power of 2 in four1 (not %d)\n", nn);
    RigelAbort();
  }
#endif
  n = nn << 1;
  j = 1;

  for(i = 1; i < n; i += 2)
  {
    if (j > i)
    {
      REAL tmp, tmp2;
			tmp = data[j]; data[j] = data[i]; data[i] = tmp;
			tmp2 = data[j+1]; data[j+1] = data[i+1]; data[i+1] = tmp2;
    }
    m = nn;
    while(m >= 2 && j > m)
    {
      j -= m;
      m >>= 1;
    }
    j += m;
  }
  //Here is the Danielson-Lanczos part
  mmax = 2;
  tblIndex = 1;
  while(n > mmax)
  {
    istep = mmax << 1;
    //theta = isign * (6.28318530717959 / mmax);
    //wtemp = REALsin(0.5*theta);
    wtemp = sinThetaLUT[tblIndex];
    wpr = -2.0f * wtemp * wtemp;
    //wpi = REALsin(theta);
    wpi = sinThetaLUT[tblIndex-1];
    wr = 1.0f;
    wi = 0.0f;
    for(m = 1; m < mmax; m += 2)
    {
      for(i = m; i <= n; i += istep)
      {
        j = i + mmax;
        tempr = wr*data[j] - wi*data[j+1];
        tempi = wr*data[j+1] + wi*data[j];
        data[j] = data[i] - tempr;
        data[j+1] = data[i+1] - tempi;
        data[i] += tempr;
        data[i+1] += tempi;
      }
			wtemp = wr;
      wr = wtemp*wpr - wi*wpi + wr;
      wi = wi*wpr + wtemp*wpi + wi;
    }
    mmax = istep;
    tblIndex++;
  }

	//Reverse elements 1..nn-1 to fit the more conventional order
	i = 1;
	j = nn-1;
	while(i < j) {
		int idx_i = 1+2*i, idx_j = 1+2*j;
		REAL tmp = data[idx_i];
		data[idx_i] = data[idx_j];
		data[idx_j] = tmp;
		REAL tmp2 = data[idx_i+1];
		data[idx_i+1] = data[idx_j+1];
		data[idx_j+1] = tmp2;
		i++; j--;
	}
}

void fft(REAL *data, const int n, const int isign)
{
  int rowstrideshift, colstrideshift;
  const REAL *myWorkspace = workspace[RigelGetThreadNum()];
  if(n <= 512)
  {
    four1(data-1, n, isign);
    return;
  }
  else
  {
    int numffts = n / 512;
    int copynumffts = numffts;
    int strideshift = 0;
    int i;
    REAL *rowptr = data;
    while (copynumffts > 1) { copynumffts >>= 1; strideshift++; }
    //Step 1: Do 1D FFTs on the 512-element "rows" of the "matrix"
    for(i = 0; i < numffts; i++)
    {
      four1(rowptr-1, 512, isign);
      rowptr += (512*2);
    }
    //Step 2/3: Transpose and twiddle factors
    //FIXME: Am I doing the twiddle factors right?
    REAL *inwalker = data;
    const REAL *end = data+(n << 1);
    REAL *outwalker = myWorkspace;
    REAL *endofrow = data+(512*2);
    i = 0;
    
    while(1)
    {
      REAL wtemp, wpr, wpi, wr, wi;
      const REAL theta = isign * ((2.0f * (REAL)M_PI * (REAL)(i+1)) / (REAL)n);
      wtemp = REALsin(0.5f*theta);
      wpr = -2.0f * wtemp * wtemp;
      wpi = REALsin(theta);
      wr = 1.0f;
      wi = 0.0f;
      while(inwalker != endofrow)
      {
        REAL tempr = *inwalker, tempi = *(inwalker-1);
        tempr = wr*tempr - wi*tempi;
        tempi = wr*i + wi*tempr;
        *outwalker = tempr;
        *(outwalker + 1) = tempi;
        inwalker += 2;
        outwalker += (numffts << 1);
        wr = (wtemp=wr)*wpr - wi*wpi + wr;
        wi = wi*wpr + wtemp*wpi + wi;
      }
      if(endofrow == end) break;
      endofrow += (512*2);
      i++;
      outwalker = myWorkspace + (i << 1);
    }
    //Step 4: FFTs on the (n/512)-element "rows" of the transposed "matrix"
    rowptr = myWorkspace;
    for(i = 0; i < 512; i++)
    {
      four1(rowptr-1, numffts, isign);
      rowptr += (numffts << 1);
    }
    //Step 4a: Copy back into data
    inwalker = myWorkspace; outwalker = data;
    int countdown = (n << 1);
    switch (countdown & 0x7) {
      case 0:        do {  *outwalker++ = *inwalker++;
      case 7:              *outwalker++ = *inwalker++;
      case 6:              *outwalker++ = *inwalker++;
      case 5:              *outwalker++ = *inwalker++;
      case 4:              *outwalker++ = *inwalker++;
      case 3:              *outwalker++ = *inwalker++;
      case 2:              *outwalker++ = *inwalker++;
      case 1:              *outwalker++ = *inwalker++;
      } while ((countdown -= 8) > 0);
    }
  }
}
    
    
void four1s(REAL *data, const int n, const unsigned int strideshift, const int isign)
{
  int nn, mmax, m, j, istep, i, tblIndex;
  REAL wtemp, wr, wpr, wpi, wi, theta, tempr, tempi;
  if (n<2 || n&(n-1))
  {
    printf("Error: n must be power of 2 in four1 (not %d)\n", n);
    RigelAbort();
  }
  nn = n << 1;
  j = 1;
  
  for(i = 1; i < nn; i += 2)
  {
    if (j > i)
    {
      REAL tmp, tmp2;
      int idxi = i << strideshift;
      int idxj = j << strideshift;
      tmp = data[idxj-1]; tmp2 = data[idxj];
      data[idxj-1] = data[idxi-1]; data[idxj] = data[idxi];
      data[idxi-1] = tmp; data[idxi] = tmp2;
    }
    m = n;
    while(m >= 2 && j > m)
    {
      j -= m;
      m >>= 1;
    }
    j += m;
  }
  //Here is the Danielson-Lanczos part
  mmax = 2;
  tblIndex = 1;
  while(nn > mmax)
  {
    istep = mmax << 1;
    //theta = isign * (6.28318530717959f / mmax);
    //wtemp = REALsin(0.5f*theta);
    wtemp = sinThetaLUT[tblIndex-1];
    wpr = -2.0f * wtemp * wtemp;
    //wpi = REALsin(theta);
    wpi = sinThetaLUT[tblIndex];
    wr = 1.0f;
    wi = 0.0f;
    for(m = 1; m < mmax; m += 2)
    {
      for(i = m; i <= n; i += istep)
      {
        j = i + mmax;
        int idxi = i << strideshift;
        int idxj = j << strideshift;
        tempr = wr*data[idxj-1] - wi*data[idxj];
        tempi = wr*data[idxj] + wi*data[idxj-1];
        data[idxj-1] = data[idxi-1] - tempr;
        data[idxj] = data[idxi] - tempi;
        data[idxi-1] += tempr;
        data[idxi] += tempi;
      }
      wr = (wtemp=wr)*wpr - wi*wpi + wr;
      wi = wi*wpr + wtemp*wpi + wi;
    }
    mmax = istep;
    tblIndex++;
  }
}

void TransposeBlockTask(TQ_TaskDesc * tdesc, REAL *in, REAL *out) {
  unsigned int i, j;
  unsigned int ilow, ihigh, jlow, jhigh;

  //Versions that use the shifts we calculated in main()
  unsigned int tmp1 = sc - sj;
  unsigned int tmp2 = tdesc->v3 >> tmp1;
  unsigned int tmp3 = tdesc->v3 - (tmp2 << tmp1);

  ilow = BLOCK_SIZE_I*tmp2;
  ihigh = MIN((ilow+BLOCK_SIZE_I),(R));
  jlow = BLOCK_SIZE_J*tmp3;
  jhigh = MIN((jlow+BLOCK_SIZE_J),(C));

  //RigelPrint((ilow << 24) | (ihigh << 16) | (jlow << 8) | jhigh);

  unsigned int twoR = R*2;
  REAL *inWalker, *outWalker;
  for(i = ilow; i < ihigh; i++)
  {
    inWalker = in + (i*2*C + 2*jlow);
    outWalker = out + (jlow*2*R + 2*i);

      unsigned int countdown = (jhigh-jlow);
      while(countdown % 8) //Bring countdown down to a multiple of 8 (4 double words)
      {
        *outWalker = *inWalker++;
        *(outWalker+1) = *inWalker++;
        outWalker += twoR;
        countdown--;
      }
      while(countdown != 0)
      {
        *outWalker = *inWalker++;
        *(outWalker+1) = *inWalker++;
        outWalker += twoR;
        *outWalker = *inWalker++;
        *(outWalker+1) = *inWalker++;
        outWalker += twoR;
        *outWalker = *inWalker++;
        *(outWalker+1) = *inWalker++;
        outWalker += twoR;
        *outWalker = *inWalker++;
        *(outWalker+1) = *inWalker++;
        outWalker += twoR;
        countdown -= 4;
      }
  }
}

void TransposeBlockTaskBackward(TQ_TaskDesc * tdesc, REAL *in, REAL *out) {
  unsigned int i, j;
  unsigned int ilow, ihigh, jlow, jhigh;

  //Versions that use the shifts we calculated in main()
  unsigned int tmp1 = sc - sj;
  unsigned int tmp2 = tdesc->v3 >> tmp1;
  unsigned int tmp3 = tdesc->v3 - (tmp2 << tmp1);

  ilow = BLOCK_SIZE_I*tmp2;
  ihigh = MIN((ilow+BLOCK_SIZE_I),(R));
  jlow = BLOCK_SIZE_J*tmp3;
  jhigh = MIN((jlow+BLOCK_SIZE_J),(C));

  //RigelPrint((ilow << 24) | (ihigh << 16) | (jlow << 8) | jhigh);

  unsigned int twoR = R*2;
  REAL *inWalker, *outWalker;
  for(i = ilow; i < ihigh; i++)
  {
    outWalker = out + (i*2*C + 2*jlow);
    inWalker = in + (jlow*2*R + 2*i);

      unsigned int countdown = (jhigh-jlow);
      while(countdown % 8) //Bring countdown down to a multiple of 8 (4 double words)
      {
        *outWalker++ = *inWalker;
        *outWalker++ = *(inWalker+1);
        inWalker += twoR;
        countdown--;
      }
      while(countdown != 0)
      {
				*outWalker++ = *inWalker;
				*outWalker++ = *(inWalker+1);
				inWalker += twoR;
				*outWalker++ = *inWalker;
        *outWalker++ = *(inWalker+1);
        inWalker += twoR;
        *outWalker++ = *inWalker;
        *outWalker++ = *(inWalker+1);
        inWalker += twoR;
        *outWalker++ = *inWalker;
        *outWalker++ = *(inWalker+1);
        inWalker += twoR;
        countdown -= 4;
      }
  }
}

  void FFTWorkerThread()
  {
    int thread = RigelGetThreadNum();
    TQ_TaskDesc tdesc;
    TQ_RetType retval;

    int ilow=0;
    int ihigh=BLOCK_SIZE_I;

    //Handle task descriptors for lazy writeback
    descriptor_buffer db;
    unsigned int dbflags = 0;
    //Initialize flags for descriptor buffer
    if(TCMM_WRB_LAZY)
      dbflags |= DESCRIPTOR_BUFFER_HANDLES_WRITEBACK;
    if(TCMM_INV_LAZY)
      dbflags |= DESCRIPTOR_BUFFER_HANDLES_INVALIDATE;
    init_descriptor_buffer(&db, dbflags, writeback_descriptor, invalidate_descriptor);

    TQ_TaskDesc td;

    if(RIGEL_LOOP(QID, R, 1, &td) != TQ_RET_SUCCESS)
    {
      printf("Enqueue failed!  R=%d, C=%d, ilow=%d, ihigh=%d\n",R,C,ilow,ihigh);
      RigelAbort();
    }

    while (1) {
#ifdef FAST_DEQUEUE
      RIGEL_DEQUEUE(QID, &tdesc, &retval);
#else
      retval = RIGEL_DEQUEUE(QID, &tdesc);
#endif
      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS)
      {
        RigelPrint(0xDEADDEAD);
        RigelAbort();
      }
      else
      {
        RigelPrint(0xFFFFFFFF);
        // Dispatch the task
        FFT_FUNC(data + (tdesc.v3 * C * 2), C, 1);

        if (TCMM_WRB_EAGER)
        {
          writeback_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_INV_EAGER)
        {
          invalidate_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
          add_descriptor(&db, (rtm_range_t *)(&tdesc));
        }
      }
    // END TASK
    }
    if (TCMM_WRB_LAZY || TCMM_INV_LAZY)
    {
      empty_all_descriptors(&db);
    }
if(thread == 0)
  RigelPrint(0x1);

    //if(RIGEL_LOOP(QID, (R*C)/(BLOCK_SIZE_I*BLOCK_SIZE_J), 1, &td) != TQ_RET_SUCCESS)
    if(RIGEL_LOOP(QID, (R*C) >> (si+sj), 1, &td) != TQ_RET_SUCCESS)
    {
      printf("Enqueue failed!  R=%d, C=%d, ilow=%d, ihigh=%d\n",R,C,ilow,ihigh);
      RigelAbort();
    }

    while (1) {
#ifdef FAST_DEQUEUE
      RIGEL_DEQUEUE(QID, &tdesc, &retval);
#else
      retval = RIGEL_DEQUEUE(QID, &tdesc);
#endif
      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS)
      {
        RigelPrint(0xDEADDEAD);
        RigelAbort();
      }
      else
      {
        RigelPrint(0xEEEEEEEE);
        // Dispatch the task
        TransposeBlockTask(&tdesc, data, data2);

        if (TCMM_WRB_EAGER)
        {
          writeback_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_INV_EAGER)
        {
          invalidate_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
          add_descriptor(&db, (rtm_range_t *)(&tdesc));
        }

      }
    // END TASK
    }
    if (TCMM_WRB_LAZY || TCMM_INV_LAZY)
    {
      empty_all_descriptors(&db);
    }

    if(RIGEL_LOOP(QID, C, 1, &td) != TQ_RET_SUCCESS)
    {
      printf("Enqueue failed!  R=%d, C=%d, ilow=%d, ihigh=%d\n",R,C,ilow,ihigh);
      RigelAbort();
    }
    
    while (1) {
#ifdef FAST_DEQUEUE
      RIGEL_DEQUEUE(QID, &tdesc, &retval);
#else
      retval = RIGEL_DEQUEUE(QID, &tdesc);
#endif
      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS)
      {
        RigelPrint(0xDEADDEAD);
        RigelAbort();
      }
      else
      {
        RigelPrint(0xDDDDDDDD);
        // Dispatch the task
        FFT_FUNC(data2 + (tdesc.v3 * R * 2), R, 1);

        if (TCMM_WRB_EAGER)
        {
          writeback_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_INV_EAGER)
        {
          invalidate_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
          add_descriptor(&db, (rtm_range_t *)(&tdesc));
        }

      }
    // END TASK
    }
    if (TCMM_WRB_LAZY || TCMM_INV_LAZY)
    {
      empty_all_descriptors(&db);
    }

if(thread == 0)
  RigelPrint(0x3);

    //if(RIGEL_LOOP(QID, (R*C)/(BLOCK_SIZE_I*BLOCK_SIZE_J), 1, &td) != TQ_RET_SUCCESS)
    if(RIGEL_LOOP(QID, (R*C) >> (si+sj), 1, &td) != TQ_RET_SUCCESS)
    {
      printf("Enqueue failed!  R=%d, C=%d, ilow=%d, ihigh=%d\n",R,C,ilow,ihigh);
      RigelAbort();
    }

    while (1) {
#ifdef FAST_DEQUEUE
      RIGEL_DEQUEUE(QID, &tdesc, &retval);
#else
      retval = RIGEL_DEQUEUE(QID, &tdesc);
#endif
      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS)
      {
        RigelPrint(0xDEADDEAD);
        RigelAbort();
      }
      else
      {
        RigelPrint(0xCCCCCCCC);
        // Dispatch the task
        TransposeBlockTaskBackward(&tdesc, data2, data);

        if (TCMM_WRB_EAGER)
        {
          writeback_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_INV_EAGER)
        {
          invalidate_descriptor((rtm_range_t *) (&tdesc));
        }
        if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
          add_descriptor(&db, (rtm_range_t *)(&tdesc));
        }

      }
    // END TASK
    }
    if (TCMM_WRB_LAZY || TCMM_INV_LAZY)
    {
      empty_all_descriptors(&db);
    }
}

int main(int argc,char *argv[])
{
  int thread = RigelGetThreadNum();
  char fileprefix[128];

  if(thread == 0)
  {
    SIM_SLEEP_ON();

    //TODO: For now we eat the RTM parameter, NONE policy is implemented.
    char dummy[128];
    fscanf(stdin, "%s", dummy);
    //RTM_SetWritebackStrategy();
    fscanf(stdin, "%d %d", &R, &C);
    //if(R != C)
    //{
    //  printf("Error: R (%d) must equal C (%d) for this wimpy implementation\n", R, C);
    //  RigelAbort();
    //}
    fscanf(stdin, "%d %d", &BLOCK_SIZE_I, &BLOCK_SIZE_J);
    fprintf(stderr, "%dx%d FFT, %dx%d transpose block sizes\n", R, C, BLOCK_SIZE_I, BLOCK_SIZE_J);
    ClearTimer(0);

    si = 0;
    unsigned int itmp = BLOCK_SIZE_I >> 1;
    while(itmp)
    {
      si++;
      itmp >>= 1;
    }
    sj = 0;
    unsigned int jtmp = BLOCK_SIZE_J >> 1;
    while(jtmp)
    {
      sj++;
      jtmp >>= 1;
    }
    sr = 0;
    unsigned int rtmp = R >> 1;
    while(rtmp)
    {
      sr++;
      rtmp >>= 1;
    }
    sc = 0;
    unsigned int ctmp = C >> 1;
    while(ctmp)
    {
      sc++;
      ctmp >>= 1;
    }
    RIGEL_INIT(QID, MAX_TASKS);
    SW_TaskQueue_Set_DATA_PARALLEL_MODE();
    SW_TaskQueue_Set_TASK_GROUP_FETCH(2);
    SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(2);
    RigelPrint(si); RigelPrint(sj); RigelPrint(sr); RigelPrint(sc);

    ALIGNED_MALLOC(R*C*2, REAL, 11, data, data);
    RigelPrint(data);

    ALIGNED_MALLOC(R*C*2, REAL, 11, data2, data2);
    RigelPrint(data2);

    ALIGNED_MALLOC(RigelGetNumThreads(), REAL*, 11, workspace, workspace);
    int i;
    for(i = 0; i < RigelGetNumThreads(); i++)
    {
      ALIGNED_MALLOC(MAX(R,C)*2, REAL, 11, workspace[i], workspace[i]);
    }

		//Read input data into 'data' array
		char inFilename[128];
    fscanf(stdin, "%s", fileprefix);
		strncpy(inFilename, fileprefix, 128);
		FILE *in = fopen(strncat(inFilename, ".in", 3), "rb");
		assert(in && "Failed to open input file");
		size_t elements_read = fread(data, sizeof(REAL), R*C*2, in);
		assert(elements_read == (R*C*2) && "Input file ended unexpectedly");
    fclose(in);    

    RigelFlushLine(&BLOCK_SIZE_I);
    RigelFlushLine(&BLOCK_SIZE_J);
    RigelFlushLine(&data);
    RigelFlushLine(&data2);
    RigelFlushLine(&R);
    RigelFlushLine(&C);
    RigelFlushLine(&si);
    RigelFlushLine(&sj);
    RigelFlushLine(&sr);
    RigelFlushLine(&sc);

    for(i = 0; i < RigelGetNumThreads(); i += 8)
    {
      RigelFlushLine(&(workspace[i]));
    }
    RigelFlushLine(&(workspace));
    
    printf("start\n");
    SIM_SLEEP_OFF();
    StartTimer(0);
    atomic_flag_set(&flag);
  }
  else
  {
    atomic_flag_spin_until_set(&flag);
  }

  FFTWorkerThread();

  if(thread == 0)
  {
    StopTimer(0);

    //Write output data2 to file
    char outFilename[128];
    strncpy(outFilename, fileprefix, 128);
		strncat(outFilename, ".out", 4);
    FILE *out = fopen(outFilename, "wb");
		//FILE *out = fopen(outFilename, "w");
    assert(out && "Failed to open output file");
    size_t elements_written = fwrite(data, sizeof(REAL), R*C*2, out);
    assert(elements_written == (R*C*2) && "Did not finish writing to output file");
		//int i, j;
		//for(i = 0; i < R; i++) {
		//	for(j = 0; j < C; j++) {
		//    fprintf(out, "%0.1f+%0.1fj ", data[(j*2)+i*(C*2)], data[(j*2)+i*(C*2)+1]);
		//	}
		//	fprintf(out, "\n");
		//}
		//for(i = 0; i < C; i++) {
		//	for(j = 0; j < R; j++) {
		//		fprintf(out, "%0.1f+%0.1fj ", data2[(j*2)+i*(R*2)], data2[(j*2)+i*(R*2)+1]);
		//	}
		//	fprintf(out, "\n");
		//}
    fclose(out);
    /*
    free(data);
    free(data2);
    int i;
    for(i = 0; i < RigelGetNumThreads(); i++)
      free(workspace[i]);
    free(workspace);
		*/
  }
  return EXIT_SUCCESS;
}
