#ifndef __RIGEL_TASKS_H__
#define __RIGEL_TASKS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <spinlock.h>
#include <rigel.h>

// XXX: New analysis framework
#ifdef PRINT_ANALYSIS_COUNTERS
  #define DEQUEUE_COUNT_BASE 0xccee0000
  #define BARRIER_COUNT_BASE 0xccff0000
 
  #define __PRINT_DEQUEUE_START()  RigelPrint(DEQUEUE_COUNT_BASE)
  #define __PRINT_DEQUEUE_SUCC()   RigelPrint(DEQUEUE_COUNT_BASE + 1 + TQ_RET_SUCCESS)
  #define __PRINT_DEQUEUE_SYNC()   RigelPrint(DEQUEUE_COUNT_BASE + 1 + TQ_RET_SYNC)
  #define __PRINT_DEQUEUE_BLOCK()  RigelPrint(DEQUEUE_COUNT_BASE + 1 + TQ_RET_BLOCK)
  #define __PRINT_DEQUEUE_OTHER()  RigelPrint(DEQUEUE_COUNT_BASE + 1 + TQ_RET_END)
  // Call in case we want to have return based on type. (retval: TQ_RetType)
  #define ANALYSIS_PRINT_DEQUEUE_START()            \
    "   xor   $28, $28, $28                     \n" \
    "   mvui  $28, %%hi(0xccee0000)             \n" \
    "   printreg $28                            \n"

  #define ANALYSIS_PRINT_DEQUEUE_END()        \
    "   xor   $28, $28, $28                     \n" \
    "   mvui  $28, %%hi(0xccee0000)             \n" \
    "   or    $28, $28, %0                      \n" \
    "   addi    $28, $28, 1                     \n" \
    "   printreg $28                            \n"

  // At the end of the barrier we print 0xccff0007 (SYNX) instead of 0xccff0002
  // (SUCC).  The start of the barrier is in the global dequeue and we print
  // 0xccff0042 to signify a starting point.
  #define ANALYSIS_PRINT_BARRIER_END()         \
    "   xor      $28, $28, $28             \n" \
    "   mvui     $28, %%hi(0xccff0000)     \n" \
    "   or       $28, $28, %0              \n" \
    "   addi     $28, $28, 1               \n" \
    "   printreg $28                       \n"

  #define ANALYSIS_PRINT_BARRIER_START()  RigelPrint(0xccff0042);
  

#else
  #define __PRINT_DEQUEUE_START()
  #define __PRINT_DEQUEUE_SUCC()
  #define __PRINT_DEQUEUE_SYNC()
  #define ANALYSIS_PRINT_DEQUEUE_START() 
  #define ANALYSIS_PRINT_DEQUEUE_END(retval) 
#endif

#if 0
	#define START_TIMER(x)  StartTimer_FAST(x)
	#define STOP_TIMER(x)   StopTimer_FAST(x)
  #define CLEAR_TIMER(x)  ClearTimer(x)
#else
	#define START_TIMER(x) 
	#define STOP_TIMER(x) 
  #define CLEAR_TIMER(x) 
#endif


typedef struct RigelBarrier_Info {
  // Count the number of cores done at each cluster, one for each cluster
  volatile rigel_line_aligned_t *local_complete;
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


////////////////////////////////////////////////////////////////////////////////
// Added for MT
////////////////////////////////////////////////////////////////////////////////

typedef struct RigelBarrierMT_Info {
  // Count the number of cores done at each cluster, one for each cluster
  volatile rigel_line_aligned_t *local_complete;
  // Count of clusters that have completed
  volatile int global_compete;
  // Used for barrier in parallel reduce; One for each core flipped at the end of
  // each reduce, one for each core
  volatile int *local_sense;
  // Reduction notification swaps depending on value of local_sense.
  volatile int global_reduce_done;
} RigelBarrierMT_Info;

// Join a full-chip barrier
void RigelBarrierMT_EnterFull(RigelBarrierMT_Info *info) __attribute__ ((__noinline__));
// Joint a per-cluster barrier
void RigelBarrierMT_EnterCluster(RigelBarrierMT_Info *info);
// Initialize a new barrier.  The info struct must exist, but it is filled in.
void RigelBarrierMT_Init(RigelBarrierMT_Info *info);
// Tear down a barrier.  The info struct is not destroyed.
void RigelBarrierMT_Destroy(RigelBarrierMT_Info *info);
////////////////////////////////////////////////////////////////////////////////

//FIXME Technically C does not have anonymous unions (C++ does).
//No idea why LLVM doesn't complain about this, but it (and our extensive
//use on the anonymity in the benchmark/library code) makes it impossible
//to enable C99 mode in LLVM.
typedef struct TQ_TaskDesc {
	union { uint32_t ip;	    uint32_t v1; };
	union { uint32_t data;      uint32_t v2; };
	union { uint32_t begin;     uint32_t v3; };
	union { uint32_t end;       uint32_t v4; };
} TQ_TaskDesc;

typedef struct TQ_TaskDescEntry {
	// First half of cache line holds the descriptor
	TQ_TaskDesc task_desc;
	// Pointer to next task in queue
	volatile struct TQ_TaskDescEntry *next;
	volatile struct TQ_TaskDescEntry *prev;
  // Use task_id to try smart prefetch.  NOT UNIQUE**
  uint32_t taskid;
	// Pad to fill a cache line
	//uint32_t pad[2];
	uint32_t pad[1];
} TQ_TaskDescEntry;

typedef struct TQ_TaskGroupDesc {
	// Holds points to a group of tasks taken from the global queue to be placed
	// on the local queue.  It is guaranteed that these cannot migrate to another
	// cluster; All tasks in a group execute on one cluster.  Period.
	TQ_TaskDescEntry *tasks[__DEFAULT_TASK_GROUP_SIZE];
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

	TQ_RET_INIT_FAIL = 5,	// Initialization failed for some reason.  Possibly no more
										// hardware contexts available
	
	TQ_RET_SYNC = 6,			// Once all of the tasks start blocking, a synchronization
										// point has been reached.  Returning this value tells the
										// core that everyone else is at the same point
} TQ_RetType;

// Since these are read-write, I am going to line align them
extern volatile TQ_TaskDescEntry *CLUSTER_taskqueue_head[__MAX_NUM_CLUSTERS*8] 
  __attribute__ ((aligned (32)));
extern volatile unsigned int GLOBAL_tq_head_index;
extern volatile unsigned int GLOBAL_tq_tail_index;
extern volatile unsigned int GLOBAL_tq_head_data;
extern volatile unsigned int GLOBAL_tq_tail_data;
extern TQ_TaskGroupDesc *GLOBAL_taskqueue_index; //[GLOBAL_TQ_SIZE/TASK_GROUP_SIZE];
extern TQ_TaskGroupDesc *GLOBAL_taskqueue_index_baseptr; //for free()
extern TQ_TaskDescEntry *GLOBAL_taskqueue_data; // [GLOBAL_TQ_SIZE];
extern TQ_TaskDescEntry *GLOBAL_taskqueue_data_baseptr; // for free()
extern volatile int LOCAL_percore_sense_enqueue_loop[__MAX_NUM_CORES*8] __attribute__ ((aligned (32)));
extern volatile rigel_line_aligned_t CLUSTER_percluster_enqueue_loop[__MAX_NUM_CLUSTERS] __attribute__ ((aligned (32)));
extern volatile int LOCAL_percore_sense[__MAX_NUM_CORES*8] __attribute__ ((aligned (32)));
extern volatile rigel_line_aligned_t CLUSTER_percore_sense[__MAX_NUM_CLUSTERS] __attribute__ ((aligned (32)));
extern volatile rigel_line_aligned_t CLUSTER_percluster_sense[__MAX_NUM_CLUSTERS] __attribute__ ((aligned (32)));
extern volatile int GLOBAL_percluster_sense;
extern volatile int GLOBAL_cluster_waitcount;
extern volatile rigel_line_aligned_t CLUSTER_core_waitcount[__MAX_NUM_CLUSTERS]
 __attribute__ ((aligned (32))) __attribute__ ((section (".rigellocks")));
extern int GLOBAL_numcores;
extern int GLOBAL_numclusters;
extern volatile int CLUSTER_ltq_lock[__MAX_NUM_CLUSTERS] __attribute__ ((aligned (32)));
// Holds the current count, used to auto increment on
extern volatile rigel_line_aligned_t CLUSTER_ltq_ticket_count[__MAX_NUM_CLUSTERS]
 __attribute__ ((aligned (32))) __attribute__ ((section (".rigellocks")));
// Holds the current number for the lock, used to poll on
extern volatile rigel_line_aligned_t CLUSTER_ltq_ticket_poll[__MAX_NUM_CLUSTERS]
 __attribute__ ((aligned (32))) __attribute__ ((section (".rigellocks")));
extern volatile rigel_line_aligned_t CLUSTER_gtq_lock[__MAX_NUM_CLUSTERS]
 __attribute__ ((aligned (32))) __attribute__ ((section (".rigellocks")));
extern spin_lock_t GLOBAL_tq_enqueue_lock;

// The number of task groups to enqueue locally before attempting to eqneue
// globally.
int 
SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(int enqueue_count);

// Only enqueue into the LTQ making RTM act almost like it is in data-parallel
// mode
void
SW_TaskQueue_Set_DATA_PARALLEL_MODE();

int
SW_TaskQueue_Set_TASK_GROUP_FETCH(int prefetch_count);

TQ_RetType 
SW_TaskQueue_Init(int qID, int max_size);

TQ_RetType 
SW_TaskQueue_End(int qID);

TQ_RetType 
SW_TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc);

// the _FAST and _SLOW Enqueue Loop variants are deprecated.
//  TQ_RetType 
// SW_TaskQueue_Loop_FAST(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc);
// TQ_RetType 
// SW_TaskQueue_Loop_SLOW(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc){

TQ_RetType 
__attribute__ ((__noinline__))
SW_TaskQueue_Loop_PARALLEL(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc); 

TQ_RetType 
__attribute__ ((__noinline__))
SW_TaskQueue_Loop_SEQUENTIAL(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc);

TQ_RetType 
SW_TaskQueue_Dequeue_FAST(int qID, TQ_TaskDesc *tdesc);

TQ_RetType 
SW_TaskQueue_Dequeue_SLOW(int qID, TQ_TaskDesc *tdesc);

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

// void TaskQueue_Init(int qID, int max_size, TQ_RetType *retval) {
#define TaskQueue_Init_FAST(qID, max_size, retval) \
	do { __asm__ ( \
				"or $25, %1, $zero; \n\t" \
				"or $26, %2, $zero; \n\t" \
				"tq.init;\n\t" \
				"or	%0, $24, $0;"\
				: "=r"(*retval) \
				:"r"(qID), "r"(max_size) \
				: "24", "25", "26" \
			); } while(0); 
#if 0
// void TaskQueue_Dequeue_FAST(int qID, TQ_TaskDesc *tdesc, TQ_RetType *retval) {
#define TaskQueue_Dequeue_FAST(qID, tdesc, retval) \
	do { __asm__ ( \
				"tq.deq;\n\t" \
				"or	%0, $24, $0; \n\t"\
				"or %1, $25, $0; \n\t"\
				"or %2, $26, $0; \n\t"\
				"or %3, $27, $0; \n\t"\
				"or %4, $28, $0;"\
				: "=r"(*retval), "=r"((tdesc)->v1), \
					"=r"((tdesc)->v2), "=r"((tdesc)->v3), \
					"=r"((tdesc)->v4)\
				: \
				: "24", "25", "26", "27", "28"\
			); } while (0);

//TQ_RetType TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc) {
#define TaskQueue_Enqueue_FAST(qID, tdesc, retval) \
	do { __asm__ (		\
				"or $25, %1, $0; \n\t"	\
				"or $26, %2, $0; \n\t"	\
				"or $27, %3, $0; \n\t"	\
				"or $28, %4, $0; \n\t"	\
				"tq.enq;\n\t" 			\
				"or	%0, $24, $0;"				\
				: "=r"(retval)					\
				: "r"((tdesc)->v1), "r"((tdesc)->v2), "r"((tdesc)->v3), "r"((tdesc)->v4)	\
				: "24", "25", "26", "27", "28"	\
	); } while (0);


#endif
#if 0
// TaskQueue_End(int qID, TQ_RetType *retval)

	__asm__ ( "tq.end;\n\t" 
				"or	%0, $24, $0;"
				: "=r"(retval)
				: 
				: "24"
			);

	return retval;
}


TQ_RetType 
TaskQueue_Loop(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc) {
	TQ_RetType retval = TQ_RET_SUCCESS;

	// IGNORED: tdesc->begin and tdesc->end
	
	__asm__ ( 
				"or $25, %1, $0; \n\t"
				"or $26, %2, $0; \n\t"
				"or $27, %3, $0; \n\t"
				"or $28, %4, $0; \n\t"
				"tq.loop;\n\t" 
				"or	%0, $24, $0; \n\t"
				: "=r"(retval)
				: "r"(tdesc->ip), "r"(tdesc->data), "r"(tot_iters), "r"(iters_per_task)
				: "24", "25", "26", "27", "28"
			);

	return retval;
}
#endif


#ifdef __cplusplus
}
#endif

#endif
