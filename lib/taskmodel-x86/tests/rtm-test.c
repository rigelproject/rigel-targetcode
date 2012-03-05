#include <stdio.h>
#include "rigel.h"
#include "rigel-tasks.h"
#include <assert.h>

// Right now we only support one queue and the QID is ignored
#define QID 0
// Sets the size of the queue.  You want to have this be at least twice as large
// as the number of tasks you expect at runtime.
#define MAX_NUM_TASKS 32

// XXX: main() here is only used to bootstrap the emulated RTM.  It is removed
// XXX:  in actual Rigel code to be replaced with rigel_main().
int main() 
{
  // Initialize the emulation framework for the RTM
  X86_TASK_MODEL_INIT();
  // Call what will be 'main()' in the actual Rigel code
  X86_TASK_MODEL_WAIT();
  return 0;
}

// XXX: This must be defined by the user and will become main in the actual
// XXX:   Rigel code that runs on RigelSim when the user moves from x86 version
int rigel_main()
{
  // Per-core core number
  int core_num = RigelGetCoreNum();
  // Task descriptor that we pull off of the TQ
  TQ_TaskDesc td;
  // Return value from RTM TQ API
  TQ_RetType retval;
  // Flag to signal completion of dispatch loop
  int done = 0;
  // Number of global phases to do
  int barrier_count = 4;

  // Initialize RTM queue
  if (0 == core_num) TaskQueue_Init(QID, MAX_NUM_TASKS);

  // XXX: Dispatch loop
  while (barrier_count > 0) {
    // XXX: Initial work enqueue
    if (0 == core_num) {
      /* XXX: Enqueue a DOALL loop of parallel tasks */
      TQ_TaskDesc tq = { 42, 21, 12, 47 };
      TaskQueue_Loop(QID, 128, 8, &tq);
    }
    done = 0;
    
    while (!done) {
      retval = TaskQueue_Dequeue(QID, &td);
      switch (retval) {
        case TQ_RET_SUCCESS:
        {
          // Yield the processor so that you can see interleavings.
          usleep(1);
          /* XXX: DISPATCH WORK */
          fprintf(stderr, "core number: %d [SUCCESS] start: %d end: %d\n",
            core_num, 
            td.begin,
            td.end);
          break;
        }
        case TQ_RET_SYNC:
        {
          /* XXX: BARRIER HIT */
          fprintf(stderr, "core number: %d [SYNC]\n", core_num);
          barrier_count--;
          done = 1;
          break;
        }
        default:
          assert(0 && "No other TQ_RetType values are supported yet.");
      }
    }
  }
  // Shutdown RTM queue
  if (0 == core_num) TaskQueue_End(QID);
}
