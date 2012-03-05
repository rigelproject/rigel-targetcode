#include "cg.h"
#include "rigel.h"
#include "rigel-math.h"
#include "rigel-string.h"
#include "rigel-tasks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
ATOMICFLAG_INIT_CLEAR(ParReduceFlag);
ATOMICFLAG_INIT_CLEAR(Init_Flag);
#include "smatrix.h"

//#ifndef SW_COHERENCE
	#define SW_COHERENCE 0
//#endif

#define PARAM_ITERS_PER_TASK 4
#define PARAM_TASK_GROUP_PREFETCH 1

// Turn on for debugging
#define DEBUG_PRINT_OUTPUT
#define DEBUG_TIMERS_ENABLED 0

#ifdef HW_TQ
  #define FAST_DEQUEUE
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

#ifdef X86_RTM
  #define RIGEL_DEQUEUE	TaskQueue_Dequeue
  #define RIGEL_ENQUEUE	TaskQueue_Enqueue
  #define RIGEL_LOOP		TaskQueue_Loop	
  #define RIGEL_INIT		TaskQueue_Init
  #define RIGEL_END			TaskQueue_End
#endif
RigelBarrier_Info bi;

void 
MatVecMul(const SparseMatrix *A, const ElemType *vecn, ElemType *vec_out) 
{
  int row, idx, col;
  ElemType data;

  for (row = 0; row < MATRIX_DIM; row++) {
    vec_out[row] = 0.0f;
    for (idx = 0; idx < SMatGetRowSize(A, row); idx++) {
      data = SMatGetElem(A, row, idx, &col);
      vec_out[row] += vecn[col] * data;
    }
  }
}

ElemType
ParallelReduce(int corenum, int numcores, ElemType val, volatile ElemType *ReduceVals) {
  TQ_TaskDesc dummy_desc;
  TQ_RetType dummy_retval;

  // XXX: Note that we no longer have a barrier *inside* the reduce.  A barrier
  // for each cluster must be reached before entering or bad things happen.

  // Special case for one core
  if (numcores == 1) return val;
  #ifdef CLUSTER_LOCAL_TQ
    #ifdef SW_COHERENCE
    // Handle cache coherence by invalidating line before hand and flushing it at
    // the end.
    RigelInvalidateLine(&GlobalReduceReturn);
    #endif
  #endif

  // Do cluster summation on core 0 for the cluster
  if ((corenum & 0x07) == 0) {
    int foo;
    ElemType sum = 0;
    ElemType val0 = ReduceVals[corenum+0];
    ElemType val1 = ReduceVals[corenum+1];
    ElemType val2 = ReduceVals[corenum+2];
    ElemType val3 = ReduceVals[corenum+3];
    ElemType val4 = ReduceVals[corenum+4];
    ElemType val5 = ReduceVals[corenum+5];
    ElemType val6 = ReduceVals[corenum+6];
    ElemType val7 = ReduceVals[corenum+7];
    sum += val0 + val1 + val2 + val3 + val4 + val5 + val6 + val7;

    // Add to count to tell core 0 to do global reduction
    ReduceVals[corenum] = sum;
    RigelAtomicINC(foo, GlobalReduceClustersDone);

    // TODO: We could in fact have all of the cores in one cluster slurp in
    // values and thus increase the MLP relative to one core suffering
    // sequential misses at the highest level of the reduction
    if (corenum == 0) {
      ElemType sum = 0;
      int i;
      int numclusters = numcores/8;
      // Spin until all clusters have partial sum done
      while ((GlobalReduceClustersDone % numclusters) != 0);

      // Unroll if worthwhile
      if (numclusters > 4) {
        for (i = 0; i < numclusters; i+=4) {
          ElemType val0 = ReduceVals[(i+0)*8];
          ElemType val1 = ReduceVals[(i+1)*8];
          ElemType val2 = ReduceVals[(i+2)*8];
          ElemType val3 = ReduceVals[(i+3)*8];
          sum += val0 + val1 + val2 + val3;
        }
      } else {
        for (i = 0; i < numclusters; i++) {
          sum += ReduceVals[i*8];
        }
      }
      GlobalReduceReturn = sum;
      #ifdef CLUSTER_LOCAL_TQ
        #ifdef SW_COHERENCE
        // Core 0 flushes it out so everyone can "see" it. Could use G.ST
        RigelWritebackLine(&GlobalReduceReturn);
        #endif
      #endif
    }
  }
  RigelBarrier_EnterFull(&bi);

  return GlobalReduceReturn;
}
//////////////////////////////////////////////////
// Actual CG Solver Code
//////////////////////////////////////////////////
void
CGMain(int corenum, int numcores,  ElemType rho) {
  int i, j;
  SparseMatrix * A = &Amat;
  // Localize alpha and do divide indepdendently
  ElemType local_alpha;
  ElemType local_rho = rho;
  ElemType local_rho_next;
  ElemType local_beta;

  int iters_per_task;
  TQ_TaskDesc enq_tdesc, deq_tdesc;
  TQ_RetType retval;

  ElemType reduceval_1;
  ElemType reduceval_2;

  // This is a tunable parameter that determines the granularity of work.
  // Since the matrices that we are working with are on the small side (1-2k
  // rows) we may want to reconsider the size here.
  iters_per_task = PARAM_ITERS_PER_TASK;

  if (corenum == 0) StartTimer(0);
  // Iterate upto MAXITER
  for (i = 0; i < MAXITER; i++) {
    // Instead of doing a new dequeue ever step, we will now do one every
    // iteration.  Each core grabs tasks off the queue in the first stage and
    // propogates the iter_task_list[] with entries.  Each phase that core
    // iterates through the task list.  Load balancing happens at each iteration
    // instead of each phase.
    int task_list_entry_cnt = 0;  // Number of tasks in the list
    task_list_entry_t task_list[MAX_TASKLIST_ENTRY];
    int list_entry = 0; // Counter used to iterate through tasks in each phase

    //////////////////////
    // XXX: PHASE 1 START
    //////////////////////
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(2); }
    if (corenum == 0) {
      RIGEL_LOOP(QID, MATRIX_DIM, iters_per_task, &enq_tdesc);
    }

    // Everyone resets their reduction variable
    ReduceVals[corenum] = 0.0f;
    while (1) {
      #ifdef FAST_DEQUEUE
      TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
      #else
      retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
      #endif

      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS) RigelAbort();
      {
        int count = deq_tdesc.end - deq_tdesc.begin;
        int start = deq_tdesc.begin;
        ElemType reduceval;
//RigelPrint(0xaaa00000);

        // Insert a new task into the list for this iteration.
        task_list[task_list_entry_cnt].count = count;
        task_list[task_list_entry_cnt].start = start;
        task_list_entry_cnt++;

        // We will need to flush out the pieces of p before making this call.
        // Each block walks over p arbitrarily depending on which values in the
        // sparse matrix are non-zero
        Task_SMVMRows(A, p, q, &deq_tdesc); 
        // Do partial dot product
        Task_VecDot(&p[start], &q[start], count, &reduceval);
        ReduceVals[corenum] += reduceval;
      }
    }

    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(2); }
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(1); }
    reduceval_1 = 
      ParallelReduce(corenum, numcores, ReduceVals[corenum], ReduceVals);
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(1); }
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(3); }

    //////////////////////
    // XXX: PHASE 2 START
    //////////////////////
    ReduceVals[corenum] = 0.0f;
    for (list_entry = 0; list_entry < task_list_entry_cnt; list_entry++) {
      int start = task_list[list_entry].start;
      int count =  task_list[list_entry].count;
      ElemType reduceval;

      // Calc ALPHA ( Made local)
      local_alpha = local_rho / reduceval_1;
      // Calc x[i+1] and Calc r[i+1]
      Task_VecScaleAdd(local_alpha, &p[start], &x[start], &x[start], count);
      Task_VecScaleAdd(-1.0 * local_alpha, &q[start], &res[start], &res[start], count);
      Task_VecDot(&res[start], &res[start], count, &reduceval); 
      ReduceVals[corenum] += reduceval;
    }

    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(3); }
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(5); }
    // XXX: BARRIER
    #ifdef FAST_DEQUEUE
    TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
    #else
    RigelBarrier_EnterFull(&bi);
    #endif
    
    // Parallel Reduce!
    reduceval_2 = 
      ParallelReduce(corenum, numcores, ReduceVals[corenum], ReduceVals);
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(5); }
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(4); }

    //////////////////////
    // XXX: PHASE 3: START
    //////////////////////
    for (list_entry = 0; list_entry < task_list_entry_cnt; list_entry++) {
      int start = task_list[list_entry].start;
      int count =  task_list[list_entry].count;

      // Calc BETA
      local_beta = reduceval_2 / local_rho;
      // Calc p[i+1]
      Task_VecScaleAdd(local_beta, &p[start], &res[start], &p[start], count);
      #ifdef CLUSTER_LOCAL_TQ
        #ifdef SW_COHERENCE
        // Handle the flushing for this iteration.  
        {
          int i;
          const int ELEM_PER_CACHE_LINE_SH = 3; // 1 << 3 == 8
          const int ELEM_PER_CACHE_LINE = 1<<ELEM_PER_CACHE_LINE_SH;

          for (i = 0; i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelFlushLine(&p[offset]);
            RigelFlushLine(&q[offset]);
            RigelFlushLine(&res[offset]);
          }
        }
        #endif
      #endif
    }

    // Exchange pointers for next iteration
    local_rho = reduceval_2;
    if (corenum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(4); }
  }
  if (corenum == 0) StopTimer(0);
}

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
  // START THREAD LOCAL
  int i; 
  int corenum = RigelGetCoreNum();
  int numcores = RigelGetNumCores(); 
  // END THREAD LOCAL
  SparseMatrix * A = &Amat;

  if (corenum == 0) {
    FILE *inFile;
    int retval;
    unsigned int MAX_FILE_SIZE = 1024*4*1024;
    int space[2];// For removing space in file
    ElemTypeListEntry *el_ptr;
    
    RIGEL_INIT(QID, MAX_TASKS);

    // Read in the number of nonzero elements and the matrix dimension off the
    // first line of the input file
    if ((inFile = fopen("matrix.bin", "r")) == NULL) assert(0 && "fopen() failed");

    // Read in up to a 4 MiB file
    if ((retval = fread(&MATRIX_DIM, sizeof(int), 1, inFile)) != 1) assert(0 && "fread() failed");
    if ((retval = fread(&NONZERO_ELEMS, sizeof(int), 1, inFile)) != 1) assert(0 && "fread() failed");
    if ((retval = fread(&space, sizeof(int), 2, inFile)) != 2) assert(0 && "fread() failed");

    el_ptr = (ElemTypeListEntry *)malloc(sizeof(ElemTypeListEntry)*(NONZERO_ELEMS+32));
    retval = ffilemap(el_ptr, 1, MAX_FILE_SIZE, inFile);
    // Skip over the first record containing MATRIX_DIM and NONZERO_ELEMS
    elem_list = &el_ptr[1];

    fprintf(stdout, "NONZERO_ELEMS: %d, MATRIX_DIM: %d\n", NONZERO_ELEMS, MATRIX_DIM);
    elem_list[NONZERO_ELEMS].value = 0.0f;
    elem_list[NONZERO_ELEMS].col = -1;
    elem_list[NONZERO_ELEMS].row = -1;

    // Set up data structures that hold the matrix information FIXME: free()
    // these and elem_list
    b = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    x = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    res = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    temp = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    p = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    q = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    __IA = (int *)malloc(sizeof(int)*(MATRIX_DIM+1));
    __IJ = (int *)malloc(sizeof(int)*NONZERO_ELEMS);
    __MatData = (ElemType *)malloc(sizeof(ElemType)*NONZERO_ELEMS);

    // Changing this will change the amount of compute done
    // This reduces the amount of computation we do and pulls out earlier than
    // convergance.
    if ((MATRIX_DIM >> 3) > 0) MAXITER = (MATRIX_DIM >> 3);
    else MAXITER =  MATRIX_DIM;

    ReduceVals = (ElemType *)calloc(numcores, sizeof(ElemType));

    for (i = 0; i < MATRIX_DIM; i++) {
      p[i] = q[i] = res[i] = 0.0f;
      x[i] = 1.0f;
    }
    // Set up the matrix
    SMatInit(A, MATRIX_DIM, MATRIX_DIM, elem_list);
    // Find a b such that the answer is xj = 100 for all j
    MatVecMul(A, x, b);
    // Reset x to an initial guess
    for (i = 0; i < MATRIX_DIM; i++) x[i] = 0.0f;
    // Caclulate r0
    MatVecMul(A, x, temp);
    for (i=0; i < MATRIX_DIM; i++) temp[i] = -1.0f * temp[i];
    Task_VecVecAdd(temp, b, res, MATRIX_DIM);
    for (i = 0; i < MATRIX_DIM; i++) p[i] = res[i];
    Task_VecDot(res, res, MATRIX_DIM, (ElemType *)(&rho));

    ClearTimer(0);    // Total time 
    ClearTimer(1);    // Reduction 1 cost
    ClearTimer(5);    // Reduction 2 cost
    ClearTimer(2);    // SMVM
    ClearTimer(3);    // Phase II
    ClearTimer(4);    // Phase III

    RigelBarrier_Init(&bi);
    SW_TaskQueue_Set_TASK_GROUP_FETCH(PARAM_TASK_GROUP_PREFETCH);

    atomic_flag_set(&Init_Flag);
  }

  atomic_flag_spin_until_set(&Init_Flag);
  // DO CG ITERATIONS IN PARALLEL
  CGMain(corenum, numcores, rho);

  RigelBarrier_EnterFull(&bi);
  // Tidy up after the end of the computation
  if (corenum == 0) RigelBarrier_Destroy(&bi);

#ifdef DEBUG_PRINT_OUTPUT
  if (corenum == 0) { for (i = 0; i < MATRIX_DIM; i++) RigelPrint(x[i]); }
#endif

  return 0;
}
