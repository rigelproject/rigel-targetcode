//This file implements a parallel version of [Conway's Game of Life](http://en.wikipedia.org/wiki/Conway's_Game_of_Life).

//It demonstrates the structure of a MIMD task-parallel program written for the [Rigel](https://rigel.crhc.illinois.edu/)
//architecture using the Rigel Task Model, a [Bulk-Synchronous Parallel](http://en.wikipedia.org/wiki/Bulk_synchronous_parallel)
//programming model implemented by a concurrent, hierarchical task queue.

//It also demonstrates several useful idioms that take advantage of Rigel- and RigelSim-specific features.

//Includes
//========
//For `fprintf`
#include <stdio.h>
//For `malloc`, `strtoul`, etc.
#include <stdlib.h>
//For `bool` (C99)
#include <stdbool.h>
//For Rigel-specific syscalls, inline ASM, and typedefs
#include "rigel.h"
//For RTM typedefs and function declarations
#include "rigel-tasks.h"
//For convenience RTM-related macro definitions like `RIGEL_LOOP`
#include "../../benchmarks/common/common.h"
//For `ALIGNED_MALLOC`
#include "../../benchmarks/common/macros.h"

//Front Matter
//============
//Global Data
//-----------

//Turn this on and off to enable/disable printouts of the Game of Life board every iteration
#define DEBUG
#undef DEBUG

//This macro declares a global variable called `flag`
//that will be initialized to '0' (clear).  All threads
//except 0 will spin on this flag, and thread 0 will
//set the flag to 1 when it has completed initialization,
//and all threads will enter the parallel section.
ATOMICFLAG_INIT_CLEAR(flag);

//RTM allows for multiple task queues to coexist, and you supply
//an argument to many RTM function calls specifying the queue ID.
//in this case, hardcode that argument to 0.
const int QID = 0;

//Global variables are unfortunately (from a software enginering perspective)
//the easiest way to make values writable from thread 0 during initialization
//and readable from all other threads afterwards.
//As you'll see later, you need to be careful about managing the L1 and L2 cache
//values of these global variables once thread 0 is done writing them and wants
//to make them globally visible.

//These two variables store the number of rows and columns in our Game of Life grid. 
int R, C;

//These two variables define the size of the block of **output** cells handled by each task.
//This could easily be set dynamically.
int BLOCK_SIZE_R, BLOCK_SIZE_C;

//Number of iterations of Game of Life
int NUM_ITERATIONS;

//Two arrays for doing out-of-place iterations

//Could be bitvectors, but this makes the indexing easier and allows you to store
//additional per-cell info (like object number)
typedef int gol_cell_t;
gol_cell_t *WORLD_IN, *WORLD_OUT;

//Define a helper macro for indexing into a 2D packed array.
//The `+2` is due to the 0-padding on the edges
#define IDX(i, j) ((j)+((i)*(C+2)))

//Task-Centric Memory Model Handling
//----------------------------------

//The mechanics of using the task-centric memory model without compiler or runtime support
//are a bit complex, but we have some helpers to make things easier.

//Bools that tell us what TCMM policy we're using.  Only one
//`TCMM_INV_*` and one `TCMM_WRB_*` should be set to 1 at a time.
int TCMM_INV_LAZY = 0;
int TCMM_INV_EAGER = 0;
int TCMM_WRB_LAZY = 0;
int TCMM_WRB_EAGER = 0;

inline void get_task_parameters(rtm_range_t *td, gol_cell_t **in, gol_cell_t **out, int *rmin, int *rmax, int *cmin, int *cmax) {
  //Extract the `in` and `out` pointers from `v1` and `v2` in the task descriptor.
  *in = (gol_cell_t *)(td->v1.v1);
  *out = (gol_cell_t *)(td->v2.v2);
  //The cells within `in` and `out` touched by the task
  //are uniquely identified by its `v3` due to the '1' argument
  //we pass to `RIGEL_LOOP()` below.
  const int tasknum = td->v3.v3;
  const int tasks_per_row = C / BLOCK_SIZE_C;
  *rmin = ((tasknum / tasks_per_row) * BLOCK_SIZE_R) + 1; //`+ 1` due to 0-padding
  *rmax = *rmin + BLOCK_SIZE_R;
  *cmin = ((tasknum % tasks_per_row) * BLOCK_SIZE_C) + 1; //`+ 1` due to 0-padding
  *cmax = *cmin + BLOCK_SIZE_C;
}

//descriptor\_buffer\_fancy.h implements a circular queue of task descriptors.
//The descriptor buffer will automatically process task descriptors as it fills up, and will
//process all task descriptors when you tell it to at the end of an interval.
#include "../../benchmarks/common/descriptor_buffer_fancy.h"

//We need to writeback all the values we wrote into the `out`
//array during a task, so that tasks next interval will
//read the new values
inline void writeback_descriptor(rtm_range_t *td)
{
  gol_cell_t *in, *out;
  int rmin, rmax, cmin, cmax;
  get_task_parameters(td, &in, &out, &rmin, &rmax, &cmin, &cmax);
  for(int i = rmin; i < rmax; i++)
    for(int j = cmin; j < cmax; j += (__CACHE_LINE_SIZE_BYTES / sizeof(gol_cell_t)))
      RigelWritebackLine(&out[IDX(i, j)]);
}

//We need to invalidate all the input values we read during
//a task, so that next time we can read updated values that
//may have been written by another thread.
inline void invalidate_descriptor(rtm_range_t *td)
{
  gol_cell_t *in, *out;
  int rmin, rmax, cmin, cmax;
  get_task_parameters(td, &in, &out, &rmin, &rmax, &cmin, &cmax);
  //The input area we read goes from (rmin-1, cmin-1) to (rmax+1, cmax+1)
  for(int i = rmin-1; i < rmax+1; i++)
    for(int j = cmin-1; j < cmax+1; j += (__CACHE_LINE_SIZE_BYTES / sizeof(gol_cell_t)))
      RigelInvalidateLine(&in[IDX(i, j)]);
}

//Task Code
//=========

//Simulate one iteration of the Game of Life for a rectangular block of cells.
//The rules are:
//
// * If a cell is live and has less than 2 live neighbors, it dies due to loneliness.
// * If a cell is live and has more than 3 live neighbors, it dies due to overcrowding.
// * If a cell is live and has 2 or 3 live neighbors, it lives.
// * If a cell is dead, it stays dead unless it has **exactly** 3 live neighbors.
void GOLTask(rtm_range_t *td) {
  gol_cell_t *in, *out;
  int rmin, rmax, cmin, cmax;
  get_task_parameters(td, &in, &out, &rmin, &rmax, &cmin, &cmax);

  for(int i = rmin; i < rmax; i++) {
    for(int j = cmin; j < cmax; j++) {
      bool live = (in[IDX(i, j)] != 0);
      int numLiveNeighbors = (in[IDX(i-1, j-1)] != 0) +
                             (in[IDX(i-1, j)] != 0) +
                             (in[IDX(i-1, j+1)] != 0) +
                             (in[IDX(i, j-1)] != 0) +
                             (in[IDX(i, j+1)] != 0) +
                             (in[IDX(i+1, j-1)] != 0) +
                             (in[IDX(i+1, j)] != 0) +
                             (in[IDX(i+1, j+1)] != 0);
      if((live && (numLiveNeighbors == 2 || numLiveNeighbors == 3)) || (!live && numLiveNeighbors == 3))
        out[IDX(i, j)] = 1;
      else
        out[IDX(i, j)] = 0;
    }
  }
}

//Worker Thread
//=============

//Every thread runs this function after thread 0 completes initialization.
void GOLWorkerThread()
{
  TQ_TaskDesc tdesc;
  TQ_RetType ret;
  //Local `WORLD_IN`/`WORLD_OUT` pointers we can swap between iterations
  gol_cell_t *in = WORLD_IN, *out = WORLD_OUT;
  //Handle task descriptors for lazy writeback
  descriptor_buffer db;
  unsigned int dbflags = 0;
  //Initialize flags for descriptor buffer
  if(TCMM_WRB_LAZY)
    dbflags |= DESCRIPTOR_BUFFER_HANDLES_WRITEBACK;
  if(TCMM_INV_LAZY)
    dbflags |= DESCRIPTOR_BUFFER_HANDLES_INVALIDATE;
  init_descriptor_buffer(&db, dbflags, writeback_descriptor, invalidate_descriptor);


  for(int i = 0; i < NUM_ITERATIONS; i++) {
    //`RIGEL_LOOP()` is a helper function that enqueues a bunch of tasks in parallel across
    //the entire chip.  It only works correctly when all threads call it, so if that
    //doesn't hold in your use case, use `SW_TaskQueue_Enqueue()` instead.

    //`RIGEL_LOOP()` enqueues a "one-dimensional" block of tasks with identical `v1` and `v2`
    //and varying `v3` and `v4`.  We pass in a task descriptor where we can set `v1` and `v2`
    //arbitrarily; these values will be propagated to all enqueued tasks.
    //In this case, it's helpful to encode the current `in` and `out` pointers in the task
    //descriptor to let the descriptor buffer know what data to flush.
    TQ_TaskDesc td;
    //FIXME Make this process TBAA-clean by defining a `union` of `gol_cell_t` and `unsigned int`
    td.v1 = (unsigned int)in;
    td.v2 = (unsigned int)out;

    //`RIGEL_LOOP()` could fail, for example, if the task queue overflows.
    if((ret = RIGEL_LOOP(QID, (R*C)/(BLOCK_SIZE_R*BLOCK_SIZE_C), 1, &td)) != TQ_RET_SUCCESS)
    {
      fprintf(stderr, "Iteration %d, enqueue returned %d\n", i, ret);
      //This call lowers to an `abort` instruction, which immediately halts the simulation.
      RigelAbort();
    }

    //Dequeue and execute tasks 
    while (1) {
      ret = RIGEL_DEQUEUE(QID, &tdesc);
      //Task queue is empty for now, try again
      if (ret == TQ_RET_BLOCK) continue;
      //Task queue is empty and we reached a barrier, go to the next interval
      if (ret == TQ_RET_SYNC) break;
      //Failure
      if (ret != TQ_RET_SUCCESS)
      {
        fprintf(stderr, "Iteration %d, dequeue returned %d\n", i, ret);
        RigelAbort();
      }

      //Dispatch the task

      //We have 128 bits of state to play with; for more complicated
      //code with many task types in a single interval, v1 could be a function pointer,
      //or an index into a table of task function pointers.

      GOLTask((rtm_range_t *)(&tdesc));

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
    // End of interval
    if (TCMM_WRB_LAZY || TCMM_INV_LAZY)
    {
      empty_all_descriptors(&db);
    }

//Print board if DEBUG is defined
#ifdef DEBUG
    if(RigelGetThreadNum() == 0) {
      for(int i = 0; i < R; i++) {
        for(int j = 0; j < C; j++) {
          fprintf(stdout, "%01d", out[IDX(i+1, j+1)]);
        }
        fprintf(stdout,"\n");
      }
      fprintf(stdout,"----------------\n");
    }
#endif
    //Swap in and out pointers
    gol_cell_t * const temp = in; in = out; out = temp;
  } //End of all intervals
}

//Main
//====

//All threads will start here once they've finished `libc` initialization.
int main(int argc,char *argv[])
{
  //Get this thread's global ID (between 0 and `RigelGetNumThreads()`).
  int thread = RigelGetThreadNum();
  //These will be stack-allocated in all threads, but will only be used by thread 0 to free
  //`WORLD_IN` and `WORLD_OUT` at the end.
  gol_cell_t *world_in_unaligned, *world_out_unaligned;
  //Initialization
  //==============
  if(thread == 0)
  {
    //Put all other threads to sleep so we don't simulate their spin-waiting and save time.
    //This is purely a simulation time optimization, it is not necessary and does not impact
    //program semantics.
    SIM_SLEEP_ON();

    //Argument Parsing
    //----------------
    assert((argc == 7 || argc == 8) && "game_of_life TCMM_STRATEGY R C BLOCK_SIZE_R BLOCK_SIZE_C ITERATIONS [RANDOM_SEED]");
    int tcmm_strategy = (int) strtol(argv[1], NULL, 10);
    switch(tcmm_strategy) {
      //Lazy Invalidate, Lazy Writeback (LILW) 
      case 0: TCMM_INV_LAZY = 1; TCMM_WRB_LAZY = 1; break;
      //Lazy Invalidate, Eager Writeback (LIEW)
      case 1: TCMM_INV_LAZY = 1; TCMM_WRB_EAGER = 1; break;
      //Eager Invalidate, Lazy Writeback (EILW)
      case 2: TCMM_INV_EAGER = 1; TCMM_WRB_LAZY = 1; break;
      //Eager Invalidate, Eager Writeback (EIEW)
      case 3: TCMM_INV_EAGER = 1; TCMM_WRB_EAGER = 1; break;
      //No software coherence actions performed, simulates ideal hardware coherence.
      case 4: break;
      //Not allowed
      default: assert(0 && "TCMM_STRATEGY not in 0-4"); break;
    }

    R = (int) strtol(argv[2], NULL, 10);
    C = (int) strtol(argv[3], NULL, 10);
    BLOCK_SIZE_R = (int) strtol(argv[4], NULL, 10);
    BLOCK_SIZE_C = (int) strtol(argv[5], NULL, 10);

    //For simplicity, make sure R and C are evenly divisible by the block size to avoid edge case code.
    assert(((R % BLOCK_SIZE_R) == 0) && "R must be evenly divisible by BLOCK_SIZE_R");
    assert(((C % BLOCK_SIZE_C) == 0) && "C must be evenly divisible by BLOCK_SIZE_C");

    NUM_ITERATIONS = (int) strtol(argv[6], NULL, 10);
    if(argc == 8) {
      int seed = (int) strtol(argv[7], NULL, 10);
      //Seed the simulator's RNG with these 32 bits of state replicated twice.
      RigelSRand(seed, seed);
    }
    else //Seed with 0
      RigelSRand(0, 0);

    //RTM Structures
    //--------------

    //Initialize task queue
    RIGEL_INIT(QID, MAX_TASKS);
    //Store all tasks in cluster-local task queues rather than pushing to a global task queue.
    //This decreases contention and increases performance if task length is uniform, otherwise
    //the decreased contention is offset by load imbalance.
    SW_TaskQueue_Set_DATA_PARALLEL_MODE();

    //Game of Life Data
    //-----------------

    //`malloc()` `WORLD_IN` and `WORLD_OUT` arrays and 2^11-byte-align them.
    //The large alignment is for better DRAM row locality.
    //Save the unaligned pointers so we can free them later.
    //We allocate an (R+2)x(C+2) array to allow for zero-padding around the edges.
    ALIGNED_MALLOC((R+2)*(C+2), gol_cell_t, 11, world_in_unaligned, WORLD_IN);
    ALIGNED_MALLOC((R+2)*(C+2), gol_cell_t, 11, world_out_unaligned, WORLD_OUT);

		//Initialize `WORLD_IN` randomly.
    for(int i = 0; i < R; i++) {
      for(int j = 0; j < C; j++) {
        //Initialize cell (i+1, j+1) with a random integer between 0 and 1 **generated by the simulator**.
        //This lowers to a `syscall` instruction, which lets the simulator generate a random number for us in 0 cycles.
        WORLD_IN[IDX(i+1, j+1)] = RigelRandInt(0, 1);
      }
    }

    //Zero-initialize edges of `WORLD_IN` and `WORLD_OUT`.
    for(int i = 0; i < R; i++) {
      //Left edges
      WORLD_IN[IDX(i, 0)] = 0;
      WORLD_OUT[IDX(i, 0)] = 0;
      //Right edges
      WORLD_IN[IDX(i, C+1)] = 0;
      WORLD_OUT[IDX(i, C+1)] = 0;
    }
    for(int j = 0; j < C; j++) {
      //Top edges
      WORLD_IN[IDX(0, j)] = 0;
      WORLD_OUT[IDX(0, j)] = 0;
      //Bottom edges
      WORLD_IN[IDX(R+1, j)] = 0;
      WORLD_OUT[IDX(R+1, j)] = 0;
    }

    //TCMM Cache Management
    //---------------------

    //Flush (writeback+invalidate in L1/L2) all data touched by thread 0.
    //This has two purposes:
    //
    // * In a real Rigel implementation with incoherent L2s, this would be
    //required for threads in other clusters to read the written values.
    // * In a simulation environment, where a program would still work if these
    //cache operations were omitted, our performance would be too good because
    //cluster 0's caches would start out super-hot.
    RigelFlushLine(&R);
    RigelFlushLine(&C);
    RigelFlushLine(&BLOCK_SIZE_R);
    RigelFlushLine(&BLOCK_SIZE_C);
    RigelFlushLine(&NUM_ITERATIONS);
    //Strictly speaking, we only need to flush the borders of `WORLD_OUT`,
    //but we flush the whole array to make the code easier to read.
    gol_cell_t *inwalker = WORLD_IN, *inend = WORLD_IN + ((R+2)*(C+2));
    gol_cell_t *outwalker = WORLD_OUT;
    while(inwalker < inend) {
      RigelFlushLine(inwalker);
      RigelFlushLine(outwalker);
      inwalker += (__CACHE_LINE_SIZE_BYTES / sizeof(*inwalker));
      outwalker += (__CACHE_LINE_SIZE_BYTES / sizeof(*outwalker));
    }
    RigelFlushLine(&WORLD_IN);
    RigelFlushLine(&WORLD_OUT);
    RigelFlushLine(&TCMM_INV_LAZY);
    RigelFlushLine(&TCMM_INV_EAGER);
    RigelFlushLine(&TCMM_WRB_LAZY);
    RigelFlushLine(&TCMM_WRB_EAGER);

    //Reset timer 0.  Timer 0 will be used to time the parallel section, and will be output
    //at the end of the simulation (look for `TIMER_0000`).
    ClearTimer(0);
    //Wake up all the other threads
    SIM_SLEEP_OFF();
    //Start timer 0.
    StartTimer(0);
    //Set the zero-initialized flag to 1, signalling the other threads to start the worker thread.)
    atomic_flag_set(&flag);
  }
  else
  {
    atomic_flag_spin_until_set(&flag);
  }

  //Parallel Section
  //================

  //All threads, including 0, call the worker thread.  The code in this function could be inline instead,
  //but the worker thread is a useful structural idiom as it separates out the parallel section.
  GOLWorkerThread();

  //Cleanup
  //=======

  //Thread 0 should stop the timer and free global dynamically-allocated memory.
  if(thread == 0)
  {
    StopTimer(0);
    free(WORLD_IN);
    free(WORLD_OUT);
  }
  return 0;
}
