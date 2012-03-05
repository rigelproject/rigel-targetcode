#include "rtm-sw.h"
#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rigellib.h"

volatile int SWTQ_TASK_GROUP_FETCH = 1;
volatile int SWTQ_DATAPAR_MODE = 0;
volatile int SWTQ_LOCAL_ENQUEUE_COUNT = 0;

// Flag that protects initialization
ATOMICFLAG_INIT_CLEAR(GLOBAL_tq_init_flag);
// Lock protecting tail of queue for inserts
SPINLOCK_INIT_OPEN(GLOBAL_tq_enqueue_lock);

// Head and tail pointer for the task queue group descriptrs
volatile unsigned int GLOBAL_tq_head_index = 0;
volatile unsigned int GLOBAL_tq_tail_index = 0;
// Head and tail pointers for the task queue descriptor pool
// TODO:  We may want separate pools per tile/global cache bank to reduce the
// contention.
volatile unsigned int GLOBAL_tq_tail_data = 0;
volatile unsigned int GLOBAL_tq_head_data = 0;
// XXX: Cluster-level task queues and associated data
// These are accessed using LDL/STC.  The first core to find them empty locks it
// and then goes to the global TQ.
//volatile rigel_line_aligned_t *CLUSTER_tq_head_idx;
//volatile rigel_line_aligned_t *CLUSTER_tq_tail_idx;
//
// Task queue itself.  
TQ_TaskGroupDesc *GLOBAL_taskqueue_index; //[__DEFAULT_GLOBAL_TQ_SIZE/__DEFAULT_TASK_GROUP_SIZE];
TQ_TaskGroupDesc *GLOBAL_taskqueue_index_baseptr; //for free()
TQ_TaskDescEntry *GLOBAL_taskqueue_data; // [__DEFAULT_GLOBAL_TQ_SIZE];
TQ_TaskDescEntry *GLOBAL_taskqueue_data_baseptr; //for free()
// Align to a cache line for each pointer.
volatile TQ_TaskDescEntry *CLUSTER_taskqueue_head[__MAX_NUM_CLUSTERS*8] __attribute__ (( aligned (32))) ;

// Barrier management for the local cluster and per cluster TODO: We may want to
// change how we access these so that we can use more local memory operations on
// the per-cluster stuff and then flush it out before it needs to be seen
// globally.  We may be touching them with global accesses now which is a waste.
volatile int LOCAL_percore_sense[__MAX_NUM_CORES*8] __attribute__ (( aligned (32))) ;
volatile rigel_line_aligned_t CLUSTER_percore_sense[__MAX_NUM_CLUSTERS] 
  __attribute__ (( aligned (32))) ;
volatile int LOCAL_percore_sense_enqueue_loop[__MAX_NUM_CORES*8] __attribute__ (( aligned (32))) ;
volatile rigel_line_aligned_t CLUSTER_percluster_enqueue_loop[__MAX_NUM_CLUSTERS] __attribute__ (( aligned (32))) ;

volatile rigel_line_aligned_t CLUSTER_percluster_sense[__MAX_NUM_CLUSTERS] __attribute__ (( aligned (32)));
volatile int GLOBAL_percluster_sense;

// Used to check if we are in a barrier
volatile int GLOBAL_cluster_waitcount0;
volatile int GLOBAL_cluster_waitcount1;
volatile rigel_line_aligned_t CLUSTER_core_waitcount[__MAX_NUM_CLUSTERS]
 __attribute__ (( aligned (32))) __attribute__ ((section (".rigellocks")));

// Set in init to reduce number of calls to RigelGetNumCores
int GLOBAL_numcores;
int GLOBAL_numclusters;
int GLOBAL_numthreads_per_cluster;
// # == 1 << SHIFT
volatile unsigned int GLOBAL_numcores_SHIFT;
volatile unsigned int GLOBAL_numclusters_SHIFT;

// FIXME: Do something more clever later for protecting LTQ
volatile rigel_line_aligned_t CLUSTER_ltq_ticket_count[__MAX_NUM_CLUSTERS]
 __attribute__ (( aligned (32))) __attribute__ ((section (".rigellocks")));
volatile rigel_line_aligned_t CLUSTER_ltq_ticket_poll[__MAX_NUM_CLUSTERS]
 __attribute__ (( aligned (32))) __attribute__ ((section (".rigellocks")));
volatile rigel_line_aligned_t CLUSTER_gtq_lock[__MAX_NUM_CLUSTERS]
 __attribute__ (( aligned (32))) __attribute__ ((section (".rigellocks")));


// Modify the number of prefetches we do
int
SW_TaskQueue_Set_TASK_GROUP_FETCH(int prefetch_count) {
  RigelGlobalStore(prefetch_count, SWTQ_TASK_GROUP_FETCH);

  return prefetch_count;
}

void
SW_TaskQueue_Set_DATA_PARALLEL_MODE() {
  RigelGlobalStore(1, SWTQ_DATAPAR_MODE);
}

int
SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(int enqueue_count) {
  RigelGlobalStore(enqueue_count, SWTQ_LOCAL_ENQUEUE_COUNT);

  return enqueue_count;
}

int
SW_TaskQueue_SetThreadsPerCluster(uint32_t threads) {
  // Fix the number of cores
  unsigned int mask = 1UL;
  unsigned int shift = 0;
  
  int num_cores = threads*GLOBAL_numclusters;
  
  while ((mask & num_cores) == 0) {
    mask = mask << 1;
    shift++;
  }
  if ((1 << shift) != num_cores) {
    return -1;
  }
  RigelGlobalStore(shift, GLOBAL_numcores_SHIFT);
  RigelGlobalStore(num_cores, GLOBAL_numcores);
  RigelGlobalStore(threads, GLOBAL_numthreads_per_cluster);
  return 0;
}
