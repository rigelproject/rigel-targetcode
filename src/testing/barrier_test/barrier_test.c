//#include "../common.h"

#include "rigel.h"
#include <math.h>
#include "rigel-string.h"
#include "rigel-tasks.h"
//#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define N 32 //Number of iterations to do before synchronization

// Right now we only support one queue and the QID is ignored
#define QID 0
// Sets the size of the queue.  You want to have this be at least twice as large
// as the number of tasks you expect at runtime.
#define MAX_NUM_TASKS 2048

RigelBarrier_Info bi;
ATOMICFLAG_INIT_CLEAR(Init_Flag);


int main()
{
  // Per-core core number
  int core_num = RigelGetCoreNum();
  // Task descriptor that we pull off of the TQ
  TQ_TaskDesc td;
  // Return value from RTM TQ API
  TQ_RetType retval;
  // Flag to signal completion of dispatch loop

  int done = 0;
  int count = 0;
  int barrier_count = 0;

  int max_cycles = 0;
  max_cycles = RigelGetNumCores() / N;
  RigelPrint(core_num);


  // Initialize RTM queue
  if (0 == core_num) {
    TaskQueue_Init(QID, MAX_NUM_TASKS);
    RigelBarrier_Init(&bi);
    atomic_flag_set(&Init_Flag);

  }
     atomic_flag_spin_until_set(&Init_Flag);

  
  // XXX: Dispatch loop
    // XXX: Initial work enqueue
      /* XXX: Enqueue a DOALL loop of parallel tasks */
    done = 0;
  
  while (barrier_count < 2){
    if (0==core_num) {
       TQ_TaskDesc tq = { 42, 21, 12, 47 };
      TaskQueue_Loop(QID, RigelGetNumCores(), 1, &tq);
      RigelPrint(0x600d600d);
    }

    done = 0;
    while (!done) {
      retval = TaskQueue_Dequeue(QID, &td);
       // Initial Barrier
 
 //RigelPrint(retval);
      switch (retval) {
        case TQ_RET_SUCCESS:
        {
          /* XXX: DISPATCH WORK */
          //fprintf(stderr, "core number: %d [SUCCESS] start: %d end: %d\n", core_num, td.begin, td.end);
         // RigelPrint(0xba100000);
         // RigelPrint(0xba1FFFFF);
          RigelBarrier_EnterFull(&bi);

          //do some work
          int i=0;
          for (i = 0; i < core_num; i++)
             count++;


          //XXX Second Barrier
        //  RigelPrint(0xba200000);
          //RigelBarrier_EnterFull(&bi);
        //  RigelPrint(0xba2FFFFF)
          break;
        }
        case TQ_RET_SYNC:
        {
          /* XXX: BARRIER HIT */
         // fprintf(stderr, "core number: %d [SYNC]\n", core_num);
          done = 1;
    
          break;
        }
        default:
          assert(0 && "No other TQ_RetType values are supported yet.");
      }  
 
    } 
    barrier_count ++;
  }
  // Shutdown RTM queue
  if (0 == core_num) TaskQueue_End(QID);

  return 0;
}

