#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rigel-tasks.h"
#include "rigel.h"
#include "rtm-x86.h"

int __done_init = 0;
// Defined in rtm-x86.h
pthread_mutex_t rtmx86_barrier_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int rtmx86_barrier_count;
int rtmx86_barrier_global_sense;
int rtmx86_barrier_local_sense[CORES_TOTAL];
// Defined in rigel-tasks.h
int GLOBAL_numcores;
int GLOBAL_numclusters;
unsigned int GLOBAL_tq_head_index;
unsigned int GLOBAL_tq_tail_index;
TQ_TaskGroupDesc *GLOBAL_taskqueue_index; 
TQ_TaskDescEntry *GLOBAL_taskqueue_data; 
TQ_TaskDescEntry *CLUSTER_taskqueue_head[NUM_CLUSTERS];


// Synchronization for the emulated RTM
static pthread_mutex_t CLUSTER_LTQ_mutex[NUM_CLUSTERS];
static pthread_mutex_t GLOBAL_GTQ_mutex = PTHREAD_MUTEX_INITIALIZER;
static int CLUSTER_wait_count[NUM_CLUSTERS];
static int GLOBAL_wait_count;
static unsigned int GLOBAL_tq_size;
// Number of tasks in the queue currently
static int GLOBAL_tq_count;
static int GLOBAL_localsense[CORES_TOTAL];
static int GLOBAL_globalsense;

// PRECONDITION: Must not be callled until the emulated core thread has been
// initialized so that the core_num_key is set.
void rtmx86_enter_barrier()
{
  int *core_num = (int *)pthread_getspecific(rtmx86_core_num_key);
  // Flip for this barrier
  rtmx86_barrier_local_sense[*core_num] = !rtmx86_barrier_local_sense[*core_num];

  // Add the thread to the barrier
  pthread_mutex_lock( &rtmx86_barrier_count_mutex );
  rtmx86_barrier_count++;
#ifdef DEBUG
  fprintf(stderr, "%s:%d (%s) barrier count: %d\n", 
    __FILE__, 
    __LINE__, 
    __FUNCTION__, 
    rtmx86_barrier_count);
#endif
  if (rtmx86_barrier_count == CORES_TOTAL) {
    // Barrier Reached
    rtmx86_barrier_count = 0;
    rtmx86_barrier_global_sense = rtmx86_barrier_local_sense[*core_num];
#ifdef DEBUG
  fprintf(stderr, "%s:%d (%s) barrier reached\n", 
    __FILE__, 
    __LINE__, 
    __FUNCTION__);
#endif
  }
  pthread_mutex_unlock( &rtmx86_barrier_count_mutex );

  // Check to see if the barrier is completed
  while (rtmx86_barrier_local_sense[*core_num] != rtmx86_barrier_global_sense);
}

void
X86_TASK_MODEL_INIT()
{
  int i;

  // One thread by core
  rtmx86_thread_id = (pthread_t *)malloc(sizeof(pthread_t)*CORES_TOTAL);
  // Generate a core number key visible to all threads.
  pthread_key_create(&rtmx86_core_num_key, NULL);
  // Initialize RTM emulation barrier (not used by Rigel code directly)
  rtmx86_barrier_count = 0;
  rtmx86_barrier_global_sense = 0;
  for (i = 0; i < CORES_TOTAL; i++) {
    rtmx86_barrier_local_sense[i] = 0;
  }
  // Initialize all of the locks and cluster-level data structures
  for (i = 0; i < NUM_CLUSTERS; i++) {
    pthread_mutex_unlock(&(CLUSTER_LTQ_mutex[i]) );
    CLUSTER_wait_count[i] = 0;
  }
  for (i = 0; i < CORES_TOTAL; i++) {
    GLOBAL_localsense[i] = 0;
  }
  GLOBAL_globalsense = 0;
  GLOBAL_wait_count = 0;

  // Start up multiple worker threads
  for(i=0; i < CORES_TOTAL; i++)
  {
    // Initialize data for the core
    rtmx86_thread_funct_data_t *data;
    // TODO: the data structure must be free'd at exit
    data = (rtmx86_thread_funct_data_t *)malloc(sizeof(rtmx86_thread_funct_data_t));
    data->core_num = i;

    // Create the core as a new thread with the new data structure
    pthread_create( 
      &rtmx86_thread_id[i], 
      NULL, 
      rtmx86_emulated_core_thread, 
      (void *)data
    );
  }
}

void
X86_TASK_MODEL_WAIT()
{
  int i;

  for(i = 0; i < CORES_TOTAL; i++) {
    pthread_join( rtmx86_thread_id[i], NULL); 
  }
}

// This is the emulated 
void *
rtmx86_emulated_core_thread(void *ptr)
{
  int *core_num;
  rtmx86_thread_funct_data_t *data = (rtmx86_thread_funct_data_t *)(ptr);

  // FIXME: This should be free'd with the destructor
  core_num = (int *)malloc(sizeof(int));
  *core_num = data->core_num;
  
  pthread_setspecific(rtmx86_core_num_key, core_num);

//  fprintf(stderr, "%s: core_num: %d\n", __FUNCTION__, *core_num);

  // XXX: CALL TO RIGEL CODE! XXX
  rigel_main();
  // XXX:    END RIGEL CODE   XXX

  return NULL;
}

// ******** librigel *********
int
RigelGetCoreNum()
{
  return *((int *)pthread_getspecific(rtmx86_core_num_key));
}

int RigelGetClusterNum()
{
  // Clusters are emulated here by group sequential threadid's of size
  // CORES_PER_CLUSTER into groups.
  int cluster_num = RigelGetCoreNum() / CORES_PER_CLUSTER;

  return cluster_num;
}

int
RigelGetNumCores() { return CORES_TOTAL; }

// ******** libtimer (TODO) *********
void ClearTimer(int timer_num) {}
void StartTimer(int timer_num) {}
void StopTimer(int timer_num) {}

// ******** spinlock.h stuff *********
void atomic_flag_set(atomic_flag_t *f) 
{
  f->flag_val = 1;
  // On x86 we want to make sure that this write is globally visible before
  // updates from other processors are.
  asm volatile ( "mfence" );
}

void atomic_flag_spin_until_set(atomic_flag_t *f) 
{
  unsigned int flagval = 0;
  do {
    flagval = f->flag_val;
    // Again, probably overkill but ensures global ordering of flag updates
    asm volatile ( "mfence" );
  } while (!flagval);
}

void atomic_flag_spin_until_clear(atomic_flag_t *f)
{
  unsigned int flagval = 1;
  do {
    flagval = f->flag_val;
    // Again, probably overkill but ensures global ordering of flag updates
    asm volatile ( "mfence" );
  } while (flagval);
}

// ******** libpar *********

void 
RigelBarrier_EnterFull(RigelBarrier_Info *info)
{
  rtmx86_enter_barrier();
}

void 
RigelBarrier_EnterCluster(RigelBarrier_Info *info)
{

}

void 
RigelBarrier_Init(RigelBarrier_Info *info)
{

}

void 
RigelBarrier_Destroy(RigelBarrier_Info *info)
{

}



void
SW_TaskQueue_Set_TASK_GROUP_FETCH(int count)
{
  // TODO!
}

TQ_RetType 
TaskQueue_Init(int qID, int max_size)
{
  int i;
  //DEBUG_HEADER(); DEBUG("Doing INIT");

  if (__done_init) assert(0 && "TaskQueue_Init called twice!");
  
  // This is not thread safe.  It should only be called once as well.
  __done_init = 1;
  GLOBAL_numcores = CORES_TOTAL;
  GLOBAL_numclusters = NUM_CLUSTERS;
  GLOBAL_tq_head_index = 0;
  GLOBAL_tq_tail_index = 0;
  GLOBAL_tq_size = max_size;


  // Initialize the task queue structures
  GLOBAL_taskqueue_data = (TQ_TaskDescEntry *)calloc(
    sizeof(TQ_TaskDescEntry), 
    max_size);
  GLOBAL_taskqueue_index = (TQ_TaskGroupDesc *)calloc(
    sizeof(TQ_TaskGroupDesc),
    max_size / TASK_GROUP_SIZE);
  // Set all of the local task queues to zero
  for (i = 0; i < NUM_CLUSTERS; i++) {
    CLUSTER_taskqueue_head[i] = NULL;
  }
  
  return TQ_RET_SUCCESS;
}

TQ_RetType 
TaskQueue_End(int qID) 
{
  free(GLOBAL_taskqueue_data);
  GLOBAL_taskqueue_data = NULL;
  free(GLOBAL_taskqueue_index);
  GLOBAL_taskqueue_index = NULL;

  return TQ_RET_SUCCESS;
}

TQ_RetType 
TaskQueue_Loop(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc)
{
  int start = 0;
    #ifdef TASK_DEBUG
    fprintf(stderr, "%s:%d (%s) ENQUEUE LOOP tot_iters: %d iters_per_task: %d \n", __FILE__, __LINE__, __FUNCTION__, tot_iters, iters_per_task);
    #endif
//  assert(iters_per_task < tot_iters && "Invalid number of iterations");
//  I have decided to allow this to make the lib more robust.  I think it
//  should work across RigelSim and x86 versions
//  if (iters_per_task > tot_iters) tot_iters = iters_per_task;
  while (start < tot_iters) {
    TQ_TaskDesc td;
    #ifdef TASK_DEBUG
    fprintf(stderr, "%s:%d (%s) ENQUEUE LOOP ITER\n", __FILE__, __LINE__, __FUNCTION__);
    #endif
    td.v1 = tdesc->v1;
    td.v2 = tdesc->v2;
    td.begin = start;
    start += iters_per_task;
    if (start > tot_iters) start = tot_iters;
    td.end = start;
    // Note: May block
    TaskQueue_Enqueue(qID, &td);
  }

  return TQ_RET_SUCCESS;
}

TQ_RetType 
TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc) 
{
  // Grab the global lock, if the GTQ is full, spin until a task is removed.
  // Insert the new task at the end when there is room and increment the task
  // counter for other tasks to see. 
  while (1) {
    pthread_mutex_lock(&GLOBAL_GTQ_mutex);
    if (GLOBAL_tq_size != GLOBAL_tq_count) {
      GLOBAL_taskqueue_data[GLOBAL_tq_tail_index].task_desc= *tdesc;
      GLOBAL_tq_tail_index = (GLOBAL_tq_tail_index + 1 ) % GLOBAL_tq_size;
      GLOBAL_tq_count++;
      pthread_mutex_unlock(&GLOBAL_GTQ_mutex);
      return TQ_RET_SUCCESS;
    }
    pthread_mutex_unlock(&GLOBAL_GTQ_mutex);
  }
  // Should never reache here
  return TQ_RET_SUCCESS;
}

TQ_RetType 
TaskQueue_Dequeue(int qID, TQ_TaskDesc *tdesc)
{
  int cluster_num = RigelGetClusterNum();
  int core_num = RigelGetCoreNum();
  unsigned int head_idx, tail_idx;
  int did_waitcount = 0;

  // Get a GTQ reservation and update the head
  pthread_mutex_lock(&GLOBAL_GTQ_mutex);
  head_idx = GLOBAL_tq_head_index;
  GLOBAL_tq_head_index++;
  if (GLOBAL_tq_head_index > GLOBAL_tq_size) {
    GLOBAL_tq_head_index = (GLOBAL_tq_head_index % GLOBAL_tq_size);
  }
  pthread_mutex_unlock(&GLOBAL_GTQ_mutex);

  while (1) {
    pthread_mutex_lock(&GLOBAL_GTQ_mutex);
    if (GLOBAL_tq_count > 0) {
      // Tasks are available!  Check to see if we have valid head index
      tail_idx = GLOBAL_tq_tail_index;

      // FIXME: Deal with wraparound case.  I will deal with it later.  For now
      // make the GTQ big enough such that wraparound does not happen.
      if (head_idx < tail_idx) {
        *tdesc = GLOBAL_taskqueue_data[head_idx].task_desc;
        GLOBAL_tq_count--;
        if (did_waitcount) {
          GLOBAL_localsense[core_num] = !GLOBAL_localsense[core_num];
          GLOBAL_wait_count--;
          assert(GLOBAL_wait_count >= 0);
        }
        pthread_mutex_unlock(&GLOBAL_GTQ_mutex);

        return TQ_RET_SUCCESS;
      }
    } else if (!did_waitcount) {
      GLOBAL_localsense[core_num] = !GLOBAL_localsense[core_num];
      // Add to barrier
      GLOBAL_wait_count++;
      did_waitcount = 1;

      // Barrier reached
      if (CORES_TOTAL == GLOBAL_wait_count) {
        #ifdef TASK_DEBUG
          fprintf(stderr, "------------------------------- BARRIER ");
          fprintf(stderr, "REACHED --------------------------------\n");
        #endif
        GLOBAL_globalsense = GLOBAL_localsense[core_num];  
        GLOBAL_wait_count = 0;
        // FIXME: This is a hack to prevent wrap around.  Since there is a
        // single lock that protects the barrier and the TQ's, we can reset the
        // head and tail pointers without fear of a race.
        GLOBAL_tq_tail_index = 0;
        GLOBAL_tq_head_index = 0;
      }
    } else if (GLOBAL_globalsense == GLOBAL_localsense[core_num]) {
      // Barrier finished! 
      pthread_mutex_unlock(&GLOBAL_GTQ_mutex);
      return TQ_RET_SYNC;
    }
    pthread_mutex_unlock(&GLOBAL_GTQ_mutex);
  }

  return TQ_RET_SUCCESS;
}

size_t
ffilemap(void *buf, size_t size, size_t nobj, FILE* fp)
{
  int bytes_read = 0;

  bytes_read = fread(buf, size, nobj, fp);

  return bytes_read;
}

