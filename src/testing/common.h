#ifndef RIGEL
  #define RIGEL
#endif

#undef DEBUG
#undef DEBUG_PRINT_OUTPUT

// XXX TODO NEAL CRAGO TODO XXX
// If defined, PARTIAL_RUN will shorten runs by only doing a few iterations.
// Applied to: dmm (JHK), cg (JHK), kmeans (JHK), heat (JHK)
// Need to apply to: sobel, convexhull, gjk, lu, mri
#define PARTIAL_RUN

// XXX TODO BILL TOUHY TODO XXX
// If define, FAST_FILE_IO will slurp in its data set from the file 'input.bin'
// in the current working directory (CWD) of the rigelsim run.
// Applied to:
// Need to apply to: sobel, convexhull, gjk, lu, mri, dmm, cg, kmeans, heat 
#undef FAST_FILE_IO

#define SW_COHERENCE 1
// Number of task_list_entry_t per core.   Since we assign one iter or more per
// tasks, we are probably going to be able to support upto 8*MAX_TASKLIST_ENTRY
// rows in the matrix which is *a lot*
#ifndef MAX_TASKS
  #define MAX_TASKS (1 << 18)
#endif

#ifdef HW_TQ
  #define FAST_DEQUEUE
  #define RIGEL_DEQUEUE TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE TaskQueue_Enqueue
  #define RIGEL_LOOP    TaskQueue_Loop  
  #define RIGEL_INIT    TaskQueue_Init
  #define RIGEL_END     TaskQueue_End
#endif

#ifdef CLUSTER_LOCAL_TQ
  #define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE SW_TaskQueue_Enqueue
//  #define RIGEL_LOOP    SW_TaskQueue_Loop_SERIAL
  #define RIGEL_LOOP    SW_TaskQueue_Loop_PARALLEL
  #define RIGEL_INIT    SW_TaskQueue_Init
  #define RIGEL_END     SW_TaskQueue_End
#endif

// Added a switch to turn on and off use of non-globally allocate prefetches.
// CG is the only benchmark that uses these right now.  If turned of, regular
// prefetches are used.
#define USE_SW_PREFETCH_NGA
