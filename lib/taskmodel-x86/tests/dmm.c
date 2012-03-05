#include "rigel.h"
#include "rigel-tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
ATOMICFLAG_INIT_CLEAR(Init_flag);

const int QID = 0;

#define SW_COHERENCE 

#ifdef HW_TQ
	#define RIGEL_DEQUEUE	TaskQueue_Dequeue_FAST
	#define RIGEL_ENQUEUE	TaskQueue_Enqueue
	#define RIGEL_LOOP		TaskQueue_Loop	
	#define RIGEL_INIT		TaskQueue_Init
	#define RIGEL_END			TaskQueue_End
#endif

#ifdef CLUSTER_LOCAL_TQ
	#define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_CLUSTER_FAST
	#define RIGEL_ENQUEUE	SW_TaskQueue_Enqueue_CLUSTER
	#define RIGEL_LOOP		SW_TaskQueue_Loop_CLUSTER_FAST
	#define RIGEL_INIT		SW_TaskQueue_Init_CLUSTER
	#define RIGEL_END			SW_TaskQueue_End_CLUSTER
#endif

#ifdef X86_RTM
  #define RIGEL_DEQUEUE	TaskQueue_Dequeue
  #define RIGEL_ENQUEUE	TaskQueue_Enqueue
  #define RIGEL_LOOP		TaskQueue_Loop	
  #define RIGEL_INIT		TaskQueue_Init
  #define RIGEL_END			TaskQueue_End
#endif

typedef int ElemType;

ElemType **matA1;
ElemType **matA2;
ElemType **resultA;

// RESULT MATRIX: 
//   _________________ <-\
//  | | |    +        |
//  | | |    +        |   MATDIM/BLOCK_COUNT_J
//  | | |    +        |
//  | | |    +        |
//  |+|+|+++++++++++++|<-/
//  |_|_|_____________|
//  | |s|    +        |< I_ITERS_PER_TASK
//  |_|_|_____________|
//  | | |    +        |     (S is the output  block)
//   _|_|_____________
//     ^      ^        ^
//  Range J    \      /
//              MATDIM/BLOCK_COUNT_I
//
// # tasks = (MATDIM / J_ITERS_PER_TASK) * (MATDIM / I_ITERS_PER_TASK)
volatile int BLOCK_SIZE_I;
volatile int BLOCK_SIZE_J;
volatile int MATDIM;
// XXX: Must be eight
const int I_ITERS_PER_TASK = 8; 
// XXX: Must be eight
const int UNROLL_FACTOR = 8;       

// XXX: Should be small; Determines task granularity.  Ideal size is 8 since
// that minimizes the number of flushes.  Less than that, and
// RigelWritebackLine is called on multiple copies of the same data wasting
// network bandwidth
const int J_ITERS_PER_TASK = 4; 

// The number of sub matrices to break the output into.  The number of blocks to
// be done is equal to BLOCK_SIZE_I * BLOCK_SIZE_J
const int BLOCK_COUNT_I = 4;
const int BLOCK_COUNT_J = 4;
const int ELEMS_PER_CACHE_LINE= 32/sizeof(ElemType);
    
// Below are a bunch of quick-and-dirty debug routines for checking that
// matrix multiply is not broken.  The two output files 'result.out' and
// 'golden.out' can be diff'ed to check for correctness.
#ifdef DEBUG_DMM
ElemType **result_golden;

static void 
simple_matrix_multiply(const ElemType **a, const ElemType **b, ElemType **result, int dim_x, int dim_y, int dim_common) 
{
  int i, j, k;

  for (i = 0; i < dim_x; i++) {
    for (j = 0; j < dim_y; j++) {
      result[i][j] = 0;
      for (k = 0; k < dim_common; k++) {
        result[i][j] += (a[i][k] * b[k][j]);
      }
 //     fprintf(stderr, "GOLDEN: %d, %d, 0x%08x\n", i, j, result[i][j]);
    }
  }
}

static void 
print_matrix(FILE *outfile, const ElemType **m, int dim_x, int dim_y)
{
  int i, j;

  assert((outfile != NULL) && "Output file is NULL!"  
    "Make sure you pass in an open file descriptor");

  for (i = 0; i < dim_x; i++) {
    for (j = 0; j < dim_y; j++) {
      fprintf(outfile, "0x%08d ", m[i][j]);
    }
    fprintf(outfile, "\n");
  }
}

static void 
print_results()
{
  FILE *result_out, *golden_out;
  if (NULL == (result_out = fopen("result.out", "w"))) RigelAbort();
  if (NULL == (golden_out = fopen("golden.out", "w"))) RigelAbort();
  //fprintf(stdout, "MATRIX A1: \n");
  //print_matrix(stdout, (const ElemType **)matA1, MATDIM, MATDIM);
  //fprintf(stdout, "MATRIX A2: \n");
  //print_matrix(stdout, (const ElemType **)matA1, MATDIM, MATDIM);
  print_matrix(result_out, (const ElemType **)resultA, MATDIM, MATDIM);
  print_matrix(golden_out, (const ElemType **)result_golden, MATDIM, MATDIM);

  fclose(result_out);
  fclose(golden_out);
}

static void
debug_dmm_init()
{
  int i, j;
  // Seed the matrices
  for (i = 0; i < MATDIM; i++) {
    for (j = 0; j < MATDIM; j++) {
      matA1[i][j] = i*j;
      matA2[i][j] = i*j+MATDIM;
      resultA[i][j] = 0;
    }
  }
  // Generate a golden reference version of the output
  result_golden = (ElemType **)malloc(sizeof(ElemType *)*MATDIM);
  for (i = 0; i < MATDIM; i++) {
  	result_golden[i] = (ElemType *)malloc(sizeof(ElemType)*MATDIM);
  } 
  simple_matrix_multiply((const ElemType **)matA1, (const ElemType **)matA2, result_golden, MATDIM, MATDIM, MATDIM);
}
#endif /* END DEBUG_DMM */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////	TASK-BASED VERSION OF MM FOR RIGEL
////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// The innermost loop is not tasked, but could be
// v2 field contains the outer loop iteration that this is a member of
void InnerLoop(TQ_TaskDesc * tdesc) {
	int i, j, k, kk;
	int mat_dim = MATDIM;

	ElemType ** mat1 = matA1;
	ElemType ** mat2 = matA2;
	ElemType ** result = resultA;

  // Iteration in MAT_A: [i_iter,i_iter+I_ITERS_PER_TASK)
	int i_iter = tdesc->v1;
  // Iteration in MAT_B: [j_start+td.begin, j_start+td.end)
  int j_start = tdesc->v2;

  // Window over MAT_2
	for (j = j_start + tdesc->begin; j < j_start + tdesc->end; j++) {
		register ElemType result_temp_sum0 = 0;
		register ElemType result_temp_sum1 = 0;
		register ElemType result_temp_sum2 = 0;
		register ElemType result_temp_sum3 = 0;
		register ElemType result_temp_sum4 = 0;
		register ElemType result_temp_sum5 = 0;
		register ElemType result_temp_sum6 = 0;
		register ElemType result_temp_sum7 = 0;			
		// Prefetch next iteration
		for (k = 0; k < mat_dim; k+=UNROLL_FACTOR) {
			register ElemType line0;
			register ElemType line1;
			register ElemType line2;
			register ElemType line3;
			register ElemType line4;
			register ElemType line5;
			register ElemType line6;
			register ElemType line7;
			// I HATE YOU LLVM!  This needs to be register promoted but LLVM will not
			// do it yet.
			ElemType result_temp[I_ITERS_PER_TASK];
			// Prefetch next iteration...
			line0 = mat2[k+0][j];
			line1 = mat2[k+1][j];
			line2 = mat2[k+2][j];
			line3 = mat2[k+3][j];
			line4 = mat2[k+4][j];
			line5 = mat2[k+5][j];
			line6 = mat2[k+6][j];
			line7 = mat2[k+7][j];
			for (kk = 0; kk < I_ITERS_PER_TASK; kk++) {
				register ElemType sum = 0;
				sum += mat1[i_iter+kk][k+0] * line0;
				sum += mat1[i_iter+kk][k+1] * line1;
				sum += mat1[i_iter+kk][k+2] * line2;
				sum += mat1[i_iter+kk][k+3] * line3;
				sum += mat1[i_iter+kk][k+4] * line4;
				sum += mat1[i_iter+kk][k+5] * line5;
				sum += mat1[i_iter+kk][k+6] * line6;
				sum += mat1[i_iter+kk][k+7] * line7;
				result_temp[kk] = sum;
			}
			result_temp_sum0 += result_temp[0];
      result_temp_sum1 += result_temp[1];
      result_temp_sum2 += result_temp[2];
      result_temp_sum3 += result_temp[3];
      result_temp_sum4 += result_temp[4];
      result_temp_sum5 += result_temp[5];
      result_temp_sum6 += result_temp[6];
      result_temp_sum7 += result_temp[7];
		}
		result[i_iter+0][j] = result_temp_sum0;
		result[i_iter+1][j] = result_temp_sum1;
		result[i_iter+2][j] = result_temp_sum2;
		result[i_iter+3][j] = result_temp_sum3;
		result[i_iter+4][j] = result_temp_sum4;
		result[i_iter+5][j] = result_temp_sum5;
		result[i_iter+6][j] = result_temp_sum6;
		result[i_iter+7][j] = result_temp_sum7;
	}

#if 0
// XXX: There is no need for writing these lines back because of the way we
// block of the matrix.  If we did the blocking on the input matrices (how we
// should do it for large matrices) such that we accumulated partial products,
// we would need to flush out all of the written data. 
#ifdef CLUSTER_LOCAL_TQ
  #ifdef SW_COHERENCE
  // Walk over the output data a cache line at a time.  Writeback and invalidate
  // since we never touch this data again.
  {
    for ( j = j_start + tdesc->begin; 
          j < j_start + tdesc->end; 
          j += ELEMS_PER_CACHE_LINE) 
    {
      RigelFlushLine(&result[i_iter+0][j]);
      RigelFlushLine(&result[i_iter+1][j]);
      RigelFlushLine(&result[i_iter+2][j]);
      RigelFlushLine(&result[i_iter+3][j]);
      RigelFlushLine(&result[i_iter+4][j]);
      RigelFlushLine(&result[i_iter+5][j]);
      RigelFlushLine(&result[i_iter+6][j]);
      RigelFlushLine(&result[i_iter+7][j]);
    }
  }
  RigelMemoryBarrier();
  #endif
#endif
#endif
}

// Chunk all of the outer loops up into smaller tasks in parallel
void BlockedMatMultTask() {
	int QID = 0;
	// FIXME: This is just to make sure we get no overlfows.  When we add proper
	// overflow modeling, it will need to be changed.
	int core = RigelGetCoreNum();
	int num_cores = RigelGetNumCores();
	TQ_TaskDesc tdesc;
	TQ_RetType retval;
  int idx_i;

  if (core == 0) {
    // Set up the global block sizes
    BLOCK_SIZE_J = MATDIM / BLOCK_COUNT_J;
    BLOCK_SIZE_I = MATDIM / BLOCK_COUNT_I;
  }

	// As long as there are tasks to process, the loop keeps going.  Note that
	// outer and inner loops may get interleaved.
	for (idx_i = 0; idx_i < MATDIM; idx_i += BLOCK_SIZE_I) {
    int idx_j;
	  for (idx_j = 0; idx_j < MATDIM; idx_j += BLOCK_SIZE_J) {
    	// Enqueue the outermost loops of the tripply-nested loop for matrix multiply
    	if (core == 0) {
    		// Insert an outerloop for almost each core.  That way we can get parallel
    		// spawning action.  It may be too much to have one outer task per core so I
    		// do it one per cluster (TUNE ME!)
    		{
    			int start_i = idx_i;
    			int end_i =  idx_i + BLOCK_SIZE_I;

    			for (start_i = idx_i; start_i < end_i; start_i += I_ITERS_PER_TASK) {
            // Each task gets one row of the A matrix and some region of the J
            // matrix.  
            // v1: The iteration that the task will do along the X dimension
            // v2: The starting j index such that the task will comput j =
            // [idx_j + tdesc.begin, idx_j + tdesc.end)
    				TQ_TaskDesc tdesc_outer  = { start_i, idx_j, 0x0, 0x0};
    				retval =
    					RIGEL_LOOP(QID, BLOCK_SIZE_J, J_ITERS_PER_TASK, &tdesc_outer);
    			}
    		}
    	}
      while (1) {
        //XXX: DEQUEUE
  	  	retval = RIGEL_DEQUEUE(QID, &tdesc);

  	  	if (retval == TQ_RET_BLOCK) continue;
  	  	if (retval == TQ_RET_SYNC) break;
  	  	if (retval != TQ_RET_SUCCESS) RigelAbort();
        {
          // Dispatch the task
          InnerLoop(&tdesc);
  	  	}
  	  	// END TASK
      }
    // SYNC FOUND, GOTO NEXT BLOCK
  	}
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////	START MAIN
////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main() 
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
	int i, j;
	int MAX_TASKS = 1 << 18;
	int core = RigelGetCoreNum();

	if (core == 0) {
		// Read input
		fscanf(stdin, "%d", &MATDIM);
		RigelPrint(0xfffa0000 | MATDIM);

		// Initialize Matrix
		matA1 = (ElemType **)malloc(sizeof(ElemType *)*MATDIM);
		for (i = 0; i < MATDIM; i++) {
			matA1[i] = (ElemType *)malloc(sizeof(ElemType)*MATDIM*2);
			matA1[i] += (32*i * 13) % MATDIM;
		}
		matA2 = (ElemType **)malloc(sizeof(ElemType *)*MATDIM);
		for (i = 0; i < MATDIM; i++) {
			matA2[i] = (ElemType *)malloc(sizeof(ElemType)*MATDIM*2);
			matA2[i] += (32*i * 17) % MATDIM;
		}
		resultA = (ElemType **)malloc(sizeof(ElemType *)*MATDIM);
		for (i = 0; i < MATDIM; i++) {
			resultA[i] = (ElemType *)malloc(sizeof(ElemType)*MATDIM*2);
			resultA[i] += (32*i*MATDIM * 19) % MATDIM;
		}
#ifdef DEBUG_DMM
    // Set up the input matrices and calculate the golden output
    debug_dmm_init();
#endif

		ClearTimer(0);
		ClearTimer(1);
    // Start full run timer
		StartTimer(0);

		RIGEL_INIT(QID, MAX_TASKS);

		atomic_flag_set(&Init_flag);
	}

	atomic_flag_spin_until_set(&Init_flag);
	BlockedMatMultTask();

  // Stop full run timer
	if (core == 0) StopTimer(0);

#ifdef DEBUG_DMM
  // This dumps both a golden version of the matrix and the parallel results.
  // The two should match and can be checked with 'diff'
  if (0 == core) print_results();
#endif
	return 0;
}

