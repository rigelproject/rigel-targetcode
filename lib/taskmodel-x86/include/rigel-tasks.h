// Note that this is the **x86** version of the rigel include file!

#ifndef __RIGEL_TASKS_H__
#define __RIGEL_TASKS_H__
#include <pthread.h>
#include <stdint.h>

/*
 * XXX: This specification is evolving.  The syntax should not change (much),
 * but the semantics may.  Look at rigel-sim/includes/task_queue.h for
 * details on what these values currently mean.
 */

// Number of tasks in a task group.  FIXME: This **MUST** be one until the
// DEQ/ENQ can deal with LTQ properly
#define TASK_GROUP_SIZE 1
// FIXME FIXME FIXME!
#define NUM_CLUSTERS 1
#define CORES_PER_CLUSTER 8
#define CORES_TOTAL (NUM_CLUSTERS*CORES_PER_CLUSTER)
#define CLUSTER_TQ_SIZE 32
#define GLOBAL_TQ_SIZE (1<<18)

#define START_TIMER(x) 
#define STOP_TIMER(x) 


typedef struct RigelBarrier_Info {
  // Count the number of cores done at each cluster, one for each cluster
  volatile int *local_complete;
  // Count of clusters that have completed
  volatile int global_compete;
  // Used for barrier in parallel reduce; One for each core flipped at the end of
  // each reduce, one for each core
  volatile int *local_sense;
  // Reduction notification swaps depending on value of local_sense.
  volatile int global_reduce_done;
} RigelBarrier_Info;

// Join a full-chip barrier
void RigelBarrier_EnterFull(RigelBarrier_Info *info) __attribute__ ((__noinline__));
// Joint a per-cluster barrier
void RigelBarrier_EnterCluster(RigelBarrier_Info *info);
// Initialize a new barrier.  The info struct must exist, but it is filled in.
void RigelBarrier_Init(RigelBarrier_Info *info);
// Tear down a barrier.  The info struct is not destroyed.
void RigelBarrier_Destroy(RigelBarrier_Info *info);


typedef struct TQ_TaskDesc {
	union { uint32_t ip; 			uint32_t v1; };
	union { uint32_t data;    uint32_t v2; };
	union { uint32_t begin;   uint32_t v3; };
	union { uint32_t end;     uint32_t v4; };
} TQ_TaskDesc;

typedef struct TQ_TaskDescEntry {
	// First half of cache line holds the descriptor
	TQ_TaskDesc task_desc;
	// Pointer to next task in queue
	volatile struct TQ_TaskDescEntry *next;
	volatile struct TQ_TaskDescEntry *prev;
	// Pad to fill a cache line
	uint32_t pad[2];
} TQ_TaskDescEntry;

typedef struct TQ_TaskGroupDesc {
	// Holds points to a group of tasks taken from the global queue to be placed
	// on the local queue.  It is guaranteed that these cannot migrate to another
	// cluster; All tasks in a group execute on one cluster.  Period.
	TQ_TaskDescEntry *tasks[TASK_GROUP_SIZE];
} TQ_TaskGroupDesc;

typedef enum TQ_RetType { 
	TQ_RET_SUCCESS = 0, 	// The returned task is valid
	
	TQ_RET_BLOCK = 1, 		// The requester must block (or in our case, try again)
	
	TQ_RET_END = 2,				// A TQ_End() was signaled while we were blocking

	TQ_RET_OVERFLOW = 3, 	// There is no space in the task queue to insert entries.
										// Handle the overflow in hardware.

	TQ_RET_UNDERFLOW = 4,	// There are tasks overflowed and the low-water mark was
										// hit. Software should fetch tasks from memory and fill the
										// queues again.

	TQ_RET_INIT_FAIL = 5,	// Initialization failed for some reasons.  Possibly no more
										// hardware contexts available
	
	TQ_RET_SYNC = 6,			// Once all of the tasks start blocking, a synchronization
										// point has been reached.  Returning this value tells the
										// core that everyone else is at the same point
} TQ_RetType;

extern unsigned int GLOBAL_tq_head_index;
extern unsigned int GLOBAL_tq_tail_index;
extern TQ_TaskGroupDesc *GLOBAL_taskqueue_index; //[GLOBAL_TQ_SIZE/TASK_GROUP_SIZE];
extern TQ_TaskDescEntry *GLOBAL_taskqueue_data; // [GLOBAL_TQ_SIZE];
extern TQ_TaskDescEntry *CLUSTER_taskqueue_head[NUM_CLUSTERS];
extern int GLOBAL_numcores;
extern int GLOBAL_numclusters;

#if 0
int
SW_TaskQueue_Set_TASK_GROUP_FETCH(int prefetch_count);

TQ_RetType 
SW_TaskQueue_Init(int qID, int max_size);

TQ_RetType 
SW_TaskQueue_End(int qID);

TQ_RetType 
SW_TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc);

TQ_RetType 
SW_TaskQueue_Loop(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc);

TQ_RetType 
SW_TaskQueue_Dequeue(int qID, TQ_TaskDesc *tdesc);
#endif


#define SW_TaskQueue_Init TaskQueue_Init
#define SW_TaskQueue_End TaskQueue_End
#define SW_TaskQueue_Enqueue TaskQueue_Enqueue
#define SW_TaskQueue_Loop TaskQueue_Loop
#define SW_TaskQueue_Dequeue TaskQueue_Dequeue


TQ_RetType 
TaskQueue_Init(int qID, int max_size);

TQ_RetType 
TaskQueue_End(int qID);

TQ_RetType 
TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc);

TQ_RetType 
TaskQueue_Loop(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc);

TQ_RetType 
TaskQueue_Dequeue(int qID, TQ_TaskDesc *tdesc);

//TODO
void
SW_TaskQueue_Set_TASK_GROUP_FETCH(int count);

#endif
