#include "rtm-sw.h"
#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rigellib.h"
#include "../../src/benchmarks/common/macros.h"

TQ_RetType 
SW_TaskQueue_Init(int qID, int max_size) 
{
  RIGEL_GLOBAL int check;
  RIGEL_ASM int num_cores, num_clusters = __MAX_NUM_CLUSTERS; 
  int i;

  check = atomic_flag_check(&GLOBAL_tq_init_flag);
  assert(!check && "TQ already initialized!");

  ALIGNED_MALLOC((__DEFAULT_GLOBAL_TQ_SIZE/__DEFAULT_TASK_GROUP_SIZE),
                 TQ_TaskGroupDesc,
                 5,
                 GLOBAL_taskqueue_index,
                 GLOBAL_taskqueue_index_baseptr);

  ALIGNED_MALLOC(__DEFAULT_GLOBAL_TQ_SIZE,
                TQ_TaskDescEntry,
                5,
                GLOBAL_taskqueue_data,
                GLOBAL_taskqueue_data_baseptr);

  RigelGlobalStore(1, SWTQ_TASK_GROUP_FETCH);

  // Get the *actual* core and cluster counts.  Use these instead of
  // __MAX_NUM_{CLUSTERS|CORES} when possible...
  // TODO: I want to remove MAX.* for this file completely if possible
  num_cores = RigelGetNumCores();
  RigelGlobalStore(num_cores, GLOBAL_numcores);
  RigelGetNumClusters(num_clusters);
  RigelGlobalStore(num_clusters, GLOBAL_numclusters);
  int numthreads_per_cluster = RigelGetNumThreadsPerCluster();
  RigelGlobalStore(numthreads_per_cluster, GLOBAL_numthreads_per_cluster);
  {
    // Get the shift for the cores.  Assumes power of two cores.
    unsigned int mask = 1UL;
    unsigned int shift = 0;
    while ((mask & num_cores) == 0) {
      mask = mask << 1;
      shift++;
    }
    asm volatile (";# NUM_CORES CHECK [START]\n");
    if ((1 << shift) != num_cores) {
      asm volatile("abort;\n");
      fprintf(stderr, "Number of cores must be power of two! cores: %d shift %d shifted value: %d", 
        num_cores, shift, (1 << shift));
    }
    asm volatile (";# NUM_CORES CHECK [DONE]\n");
    RigelGlobalStore(shift, GLOBAL_numcores_SHIFT);

    // Get the shift for the clusters.  Assumes power of two clusters.
    mask = 1UL;
    shift = 0;
    while ((mask & num_clusters) == 0) {
      mask = mask << 1;
      shift++;
    }
    asm volatile (";# NUM_CLUSTERS CHECK [START]\n");
    if ((1 << shift) != num_clusters) {
      fprintf(stderr, "Number of clusters must be power of two! clusters: %d shift %d shifted value: %d", 
        num_clusters, shift, (1 << shift));
      assert(0);
    }
    asm volatile (";# NUM_CLUSTERS CHECK [END]\n");
    RigelGlobalStore(shift, GLOBAL_numclusters_SHIFT);
  }
  // Reset all of the indices to zero
  RigelGlobalStore(0, GLOBAL_tq_head_data);
  RigelGlobalStore(0, GLOBAL_tq_tail_data);
  RigelGlobalStore(0, GLOBAL_tq_head_index);
  RigelGlobalStore(0, GLOBAL_tq_tail_index);
  // Default sense and wait counter are also zero
  RigelGlobalStore(0, GLOBAL_percluster_sense);
  RigelGlobalStore(0, GLOBAL_cluster_waitcount0);
  RigelGlobalStore(0, GLOBAL_cluster_waitcount1);

  for (i = 0; i < num_cores; i++) {
    LOCAL_percore_sense[i] = 0;
    LOCAL_percore_sense_enqueue_loop[i] = 0;
    if ((i & 0x7) == 7) {
      RigelWritebackLine(&LOCAL_percore_sense[i]);
      RigelWritebackLine(&LOCAL_percore_sense_enqueue_loop[i]);
    }
  }

  for ( i = 0; i < __MAX_NUM_CLUSTERS; i++) {
    CLUSTER_percluster_sense[i].val = 0;
    CLUSTER_percore_sense[i].val = 0;
    CLUSTER_percluster_enqueue_loop[i].val = 0;
    CLUSTER_ltq_ticket_count[i].val = 0;
    // Must be +1 from count so first locker sees it
    CLUSTER_ltq_ticket_poll[i].val = 1;
    CLUSTER_gtq_lock[i].val = 0;
    CLUSTER_core_waitcount[i].val = 0;

  }

  atomic_flag_set(&GLOBAL_tq_init_flag);

  return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_End(int qID) 
{
  int check;

  check = atomic_flag_check(&GLOBAL_tq_init_flag);
  assert(check && "TQ not initialized but END called!");

  atomic_flag_clear(&GLOBAL_tq_init_flag);

  free(GLOBAL_taskqueue_data_baseptr);
  free(GLOBAL_taskqueue_index_baseptr);
  
  return TQ_RET_SUCCESS;
}

