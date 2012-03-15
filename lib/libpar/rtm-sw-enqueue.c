#include "rtm-sw.h"
#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rigellib.h"

// TODO: Modify so that single enqueues happen at local TQ unless overflow
// occurs.  
TQ_RetType 
SW_TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc) 
{
  return SW_TaskQueue_Loop_SEQUENTIAL(qID, 1, 1, tdesc);
  //assert(0 && "Unimplemented");
  //return TQ_RET_SUCCESS;
}

TQ_RetType 
__attribute__ ((__noinline__))
SW_TaskQueue_Loop_SEQUENTIAL(int qID, int tot_iters, int iters_per_task, 
  const TQ_TaskDesc *tdesc) 
{
  int start = 0;
  int iters_per_enque = tot_iters;
  int thread_num = RigelGetThreadNum();

  // TODO: [[REMOVE ME]] We can take this out if the application is already
  // doing this check and ensuring mutual exclusion.
  if (thread_num != 0) return TQ_RET_SUCCESS;

// XXX: NOTE THAT A TASK GROUP IS ASSUMED TO BE EIGHT!!
  // Do a block enqueue
//PRINT_HISTOGRAM(0xccdd0001);
  spin_lock(&GLOBAL_tq_enqueue_lock);
  // XXX: ACQUIRE LOCK [[ENQUEUE]]
  {
    asm volatile (
      /* Grab the tail pointer and put in r20         */
      "   mvui    $20, %%hi(GLOBAL_tq_tail_index);    \n"
      "   addi    $20, $20, GLOBAL_tq_tail_index      \n"
      /* data_idx (r20) = tail_idx */
      "   g.ldw   $20, $20, 0;                        \n"
      /* Since we count by cachelines, must mul by 32 */
      "   slli    $20, $20, 8;                        \n"
      /* Get GLOBAL_taskqueue_data (r21)              */
      "   mvui    $21, %%hi(GLOBAL_taskqueue_data);   \n"
      "   addi    $21, $21, GLOBAL_taskqueue_data;    \n"
      "   ldw     $21, $21, 0                         \n"
      /* Get GLOBAL_taskqueue_index (r22)             */
      "   mvui    $22, %%hi(GLOBAL_taskqueue_index);  \n"
      "   addi    $22, $22, GLOBAL_taskqueue_index;   \n"
      "   ldw     $22, $22, 0                         \n"
      /* start (r23)                                  */
      "   xor     $23, $23, $23                       \n"
      /*********** WHILE LOOP BEGIN ********************/
      "enq_while_%=:                                  \n"
      "   sub     $24, $23, %0                         \n"
      "   ble     $24, enq_while_end_%=               \n"
      /* task_count (r24)                             */
      "   xor     $24, $24, $24;                      \n"
      /************ FOR LOOP BEGIN ********************/
      "enq_for_%=:                                    \n"
      "   addi    $25, $24, -8                        \n"
      /* BRANCH: (task_count < __DEFAULT_TASK_GROUP_SIZE)       */
      "   bge     $25, enq_for_end_%=;                \n"
      "   sub     $25, %0, $23                        \n"
      /* BRANCH: (start < tot_iters)                  */
      "   bge     $25, enq_for_end_%=;                \n" 
      /* Get tq_data_offset (r26) = data_idx %        */   
      /*    (__DEFAULT_GLOBAL_TQ_SIZE)                          */
      "   mvui    $26, 0x007F                         \n"
      /* r26 <- __DEFAULT_GLOBAL_TQ_SIZE (= 1<<18)              */
      "   ori     $26, $26, 0xFFE0                    \n"
      /* tdo = data_idx & (__DEFAULT_GLOBAL_TQ_SIZE - 1)        */
      "   and     $20, $20, $26                       \n"
      /* Get pointer to new task desc (r26)           */
      "   add     $26, $21, $20                       \n"     
      /************** INSERT TASK *********************/
      /*  tdesc.v1 <- in.v1                           */
      "   stw      %2,  $26,  0                       \n"
      /*  tdesc.v2 <- in.v2                           */
      "   stw      %3,  $26,  4                       \n"
      /*  tdesc.begin <- start                        */
      "   stw      $23, $26,  8                       \n"
      /* tdesc.end = (start += iters_per_task)        */
      "   add      $23, $23, %1                       \n"
      "   sub      $25, %0, $23                       \n"
      "   blt      $25, tot_iters_lt%=;               \n"
      "   addi     $23, %0, 0                         \n" // Set start = tot_iters
      "tot_iters_lt%=:                                \n"
      "   stw      $23, $26, 12                       \n"  // tdesc.end
      "   addi     $25, $26, 32                       \n"  // pointer to next task decriptor
      "   stw      $25, $26, 16                       \n"  // tdesc.next
      /* Clear the line from teh cluster cache        */
      "   line.wb  $26;                               \n"
      /* Insert index                                 */
      "   srli     $25, $20, 8                        \n"   // 
      "   slli     $25, $25, 5                        \n"   // tq_idx_offset: +32 every 8 cycles
      "   add      $25, $25, $22                      \n"   // GLOBAL_taskqueue_index (r22) + tq_idx_offset * 4
      "   srli     $27, $20, 3                        \n"   // idx: +4 every cycle
      "   andi     $27, $27, 0x001C                   \n"   // Only want 0..8 << 2 indexing into the cache line
      "   add      $25, $27, $25                      \n"   // GLOBAL_taskqueue_index[tq_idx_offset].tasks[index]
      "   stw      $26, $25, 0                        \n"   //    = &this task
      /* for loop updates (data_idx, task_count)      */
      "   addi  $20, $20, 32                          \n"   // data_idx++
      "   addi  $24, $24, 1                           \n"   // task_count++
      "   jmp   enq_for_%=;                           \n"
      "enq_for_end_%=:                                \n"
      /* Add a null pointer to the end of the task list XXX: Note that r26 is
       * assumed to point to the current ask in GLOBAL_taskqueue_data at this
       * point -  Do last task.next = NULL */
      "   stw   $zero, $26, 16;                       \n"
      /* increment tail pointer making task available */
      "   mvui    $28, %%hi(GLOBAL_tq_tail_index)     \n"
      "   addi    $28, $28, GLOBAL_tq_tail_index      \n"
      "   atom.inc  $28, $28, 0                       \n"
      "   jmp enq_while_%=;                           \n"
      /* Done with while() loop                       */
      "enq_while_end_%=:                              \n"
      "   ldw     $20, $sp, 4                         \n"
      "   ldw     $21, $sp, 8                         \n"
      "   ldw     $22, $sp, 12                        \n"
      "   ldw     $23, $sp, 16                        \n"
      "   ldw     $24, $sp, 20                        \n"
      "   ldw     $25, $sp, 24                        \n"
      "   ldw     $26, $sp, 28                        \n"
      "   ldw     $27, $sp, 32                        \n"
      "   ldw     $28, $sp, 36                        \n"
      "   addi    $sp, $sp, 44                        \n"
      : /* OUTPUT */
        /* INPUT */
      : "r"(iters_per_enque),  // %0
        "r"(iters_per_task),   // %1
        "r"(tdesc->v1),        // %2
        "r"(tdesc->v2)         // %3
      : "20", "21", "22", "23", "24", "25", "26", "27", "28", "memory"
    );
  }
  // XXX: RELEASE LOCK [[ENQUEUE]]
  spin_unlock(&GLOBAL_tq_enqueue_lock);
//PRINT_HISTOGRAM(0xccdd0002);


  return TQ_RET_SUCCESS;
}

// RigelPrint(0xddd00000 | cluster_num);
// RigelPrint(0xddd10000 | thread_num);
// RigelPrint(0xddd20000 | task_groups_total);
// RigelPrint(0xddd30000 | per_cluster_task_groups);
// RigelPrint(0xddd40000 | per_cluster_iter_start);
// RigelPrint(0xddd50000 | per_cluster_iter_end);
// RigelPrint(0xddd60000 | tot_iters);
// RigelPrint(0xddd70000 | iters_per_task);
// RigelPrint(0xddd80000 | tq_base_offset);     

TQ_RetType 
SW_TaskQueue_Loop_PARALLEL(int qID, int tot_iters, int iters_per_task, 
  const TQ_TaskDesc *tdesc) 
{
  unsigned int thread_num = RigelGetThreadNum();
  unsigned int cluster_num = RigelGetClusterNum();
  unsigned int has_global_lock = 0;

//RigelPrint(0xbcda0004);    
//RigelPrint(LOCAL_percore_sense_enqueue_loop);
  {
    // Attempt to get the global lock for the cluster.  There is an assumption
    // that this is only called once per barrier.  It will mostly work
    // otherwise, but the race described below might still occur.
    //
    // Check the sense to make sure another core did not already get the lock,
    // fill the task queue, and move on before this core got to the enqueue
    // function.  There would be a race here otherwise.
    //
    // If the core coming
    // through gets the GTQ lock, it will begin generating task group
    // descriptors. In normal mode, it will check for new tasks at the end of
    // each iteration and insert locally if it is empty.  
    //
    // If we fail to get the lock, try again.
    asm volatile (
        "   mvui  $27, %%hi(CLUSTER_gtq_lock); \n"
        "   addi  $27,  $27, CLUSTER_gtq_lock;  \n"
        "   mfsr  $26, $4;                  \n"
        // Note CLUSTER_gtq_lock is of type rigel_line_aligned_t
        "   slli  $26, $26, 5;              \n"
        "   add   $27, $26, $27;              \n"
        "   xor   %0, %0, %0;               \n"
        "                                   \n"
        "L%=_acq_lock:                      \n"
        "   ldl   $26, $27;              \n"
        "   bne   $26, $zero, L%=_exit;     \n" // Lock is not free
        "   addi  $26, $zero, 1;            \n" 
        "   stc   $1, $26, $27;              \n" // Attempt to set lock
        "   beq   $1, $zero, L%=_acq_lock;  \n" // Lock free, STC Failed
        "   ori   %0, $zero, 1              \n" // Success
        "                                   \n"
        "L%=_exit:                          \n" 
      : "=r"(has_global_lock) 
      :
      : "27", "26", "1", "memory"
        );
  }
//PRINT_HISTOGRAM(0xccdd0001);

//RigelPrint(0xaaccbba1);
//RigelPrint(LOCAL_percore_sense_enqueue_loop);
  // Flip local sense.  Every core flips its local sense.
  LOCAL_percore_sense_enqueue_loop[thread_num]  = 
    !LOCAL_percore_sense_enqueue_loop[thread_num];
//RigelPrint(0xaaccbba2);
//RigelPrint(LOCAL_percore_sense_enqueue_loop);

  // Only allow core zero from each cluster to proceed
  //if ( (thread_num % __NUM_CORES_PER_CLUSTER) == 0)
  //There is only one GTQ lock per cluster so this ensures that there is only
  //one enqueuer per cluster.
  if (has_global_lock)
  {
    // Only enqueue if no other core has this interval.  Regardless, we will
    // need to drop the global lock
    if (CLUSTER_percluster_enqueue_loop[cluster_num].val != 
      LOCAL_percore_sense_enqueue_loop[thread_num])
    {
      // Number of groups we will fetch across all clusters
      unsigned int task_groups_total = 0; 
      // The number of tasks that this cluster must do
      unsigned int per_cluster_task_groups = 0;
      unsigned int per_cluster_iter_start = 0;
      unsigned int per_cluster_iter_end = 0;
      int tot_tasks = 0;
      // Set the number of tasks to insert locally first.
      int local_enqueue_count = SWTQ_LOCAL_ENQUEUE_COUNT;

      // This core did all of the enqueuing, flip the cluster-level sense.  Anyone
      // else that grabs the GTQ lock after this point will see that enqueue has
      // already been done and just slip through.
      CLUSTER_percluster_enqueue_loop[cluster_num].val = 
        !CLUSTER_percluster_enqueue_loop[cluster_num].val;


      // FIXME!!! We could probably do much better with this
      //tot_tasks = tot_iters / iters_per_task;
      switch (iters_per_task) 
      {
          case 1:   tot_tasks = tot_iters;                  break;
          case 2:   tot_tasks = tot_iters >> 1;             break;
          case 4:   tot_tasks = tot_iters >> 2;             break;
          case 8:   tot_tasks = tot_iters >> 3;             break;
          default:  tot_tasks = tot_iters / iters_per_task; break;
      }

      task_groups_total = (tot_tasks + __DEFAULT_TASK_GROUP_SIZE - 1) 
                                            / __DEFAULT_TASK_GROUP_SIZE;
      per_cluster_task_groups = 
            (task_groups_total + GLOBAL_numclusters - 1) >> GLOBAL_numclusters_SHIFT;

      per_cluster_iter_start = per_cluster_task_groups * cluster_num 
                        * iters_per_task * __DEFAULT_TASK_GROUP_SIZE;
      per_cluster_iter_end = per_cluster_task_groups * (cluster_num+1)
                        * iters_per_task * __DEFAULT_TASK_GROUP_SIZE;

      // Check to make sure there is work for us to do
      if (per_cluster_iter_start >= tot_iters) {
        // XXX: [[LOCK RELEASED]]
        CLUSTER_gtq_lock[cluster_num].val = 0;
//PRINT_HISTOGRAM(0xccdd0002);
        return TQ_RET_SUCCESS;
      }

      // Adjust ending iteration based on cluster number
      if (per_cluster_iter_end > tot_iters) per_cluster_iter_end = tot_iters;



      // Do a block enqueue
      {
        TQ_TaskDesc t;
        unsigned int tq_base_offset = 0;
        unsigned int tq_group_offset = 0;
        unsigned int iter = 0;
        
        t.v1 = tdesc->v1;
        t.v2 = tdesc->v2;
        t.v3 = 0;
        t.v4 = 0;
        
        // Acquire a block of tasks descriptors equal to the number of tasks that we
        // need to enqueue.  After we are done, we own the range of tasks in:
        // [index,index+task_groups_total) all in GLOBAL_taskqueue_data[].
        // tq_base_offset is loaded once.
        TQ_DESC_POOL_GET(per_cluster_task_groups, tq_base_offset);

        // STAGE 1: Fill in the task descriptors for each task in the task group we
        // just grabbed.   This will get skipped if there are no tasks for the
        // cluster to do.
        while (per_cluster_iter_start < per_cluster_iter_end) {
          unsigned int iter = 0;
          unsigned int task_count = 0;
          unsigned int tq_offset = tq_base_offset+tq_group_offset;
          
// START ENQUEUE TASK GROUP        
//RigelPrint(0xbcda0005);    

          for ( iter = 0; (iter < __DEFAULT_TASK_GROUP_SIZE) 
              && (per_cluster_iter_start < per_cluster_iter_end); iter++)
          {
            // Calculate the offset into the task descriptor pool
            unsigned int tq_data_offset = tq_offset+iter;
            // Calculate iteration bounds for task
            t.begin = per_cluster_iter_start;
            per_cluster_iter_start += iters_per_task;
            // FIXME I do not think this check is necessary any more...
            if (per_cluster_iter_start > per_cluster_iter_end) { 
              per_cluster_iter_start = per_cluster_iter_end;
            }
            t.end = per_cluster_iter_start;
        
            // At this point we own an entry, update that entry in the data array
            GLOBAL_taskqueue_data[tq_data_offset].task_desc = t;
            GLOBAL_taskqueue_data[tq_data_offset].next 
              = &GLOBAL_taskqueue_data[tq_data_offset+1];
            // Write back the task descriptor to make it globally-visible Note: 1
            // (look below for details)
            RigelWritebackLine(&GLOBAL_taskqueue_data[tq_data_offset]);
EVENTTRACK_ENQUEUE_GROUP_START();
          }
          // Keep the total count of iterations we did in stage one.
          task_count = iter;

          LOCAL_LOCK_ACQUIRE();
          /* [[ACQUIRED LTQ LOCK]] -- Release after local enqueue*/

          // We first check to see if the LTQ is empty.  If so, add the
          // just-created TG into the LTQ and skip enqueuing to the GTQ.
          if (  NULL == CLUSTER_taskqueue_head[8*cluster_num] // Nothing locally
                || local_enqueue_count-- > 0                // Have not reached
                                                            // min LTQ enq. count yet
                || SWTQ_DATAPAR_MODE)                       // We are in
                                                            // data-parallel mode, no GTQ usage
          {
            // Grab local task queue lock before inserting into the queue.
            // Just in case another task group was inserted in the meantime we
            // grab the lock.   
            GLOBAL_taskqueue_data[tq_offset+task_count-1].next 
              = CLUSTER_taskqueue_head[cluster_num*8];
              
            //  = CLUSTER_taskqueue_head[cluster_num];
            CLUSTER_taskqueue_head[cluster_num*8] 
              = &GLOBAL_taskqueue_data[tq_offset];
 
            // Go to the next task group and skip GTQ enqueue
            /* [[RELEASE LTQ LOCK]] */
            LOCAL_LOCK_RELEASE()
          } else {
            /* [[RELEASE LTQ LOCK]] */
            LOCAL_LOCK_RELEASE()
            // Note 1: Since we elect to not invalidate the task descriptors
            // above, we must do it here.  We agressively invalidate here since it
            // generates code faster.
            for ( iter = 0; (iter < __DEFAULT_TASK_GROUP_SIZE); iter++) {
              RigelInvalidateLine(&GLOBAL_taskqueue_data[tq_offset+iter]);
            }
              
            #define __USE_INLINE_ASM_CRITICAL_SECTION
            #ifndef __USE_INLINE_ASM_CRITICAL_SECTION
            asm volatile (" #; [[ENQUEUE LOOP ]] BEGIN NON-INLINE ASM CRIT SECTION  ");
                    // STAGE 2: Second half of insertion happens with lock held.  We need to
                    // obtain a task group index, fill it in, and unlock.
                    spin_lock(&GLOBAL_tq_enqueue_lock);
                    // XXX: ACQUIRE LOCK [[ENQUEUE]]
                    {
                      unsigned int tail_idx = 0;
                      // Holds the offset into the array of tasks in memory. 
                      unsigned int tq_data_offset = 0;
            
                      // Get the current task queue tail pointing to the last TQ_TaskGroupDesc
                      TQ_INDEX_ARRAY_TAIL_GET(tail_idx);
            
                      // Update the group descriptor with the pointers to our new task
                      // descriptors.
                      for (iter = 0; iter < task_count; iter++)
                      {
                      tq_data_offset = tq_offset+iter;
                        // Insert the task descriptor into the task group
                        GLOBAL_taskqueue_index[tail_idx].tasks[iter] = 
                          &GLOBAL_taskqueue_data[tq_data_offset];
                      }
                      // The last one needs to be adjusted to point to NULL.  This handles the
                      // case where the number of tasks != a multiple of the
                      // __DEFAULT_TASK_GROUP_SIZE
                      GLOBAL_taskqueue_data[tq_data_offset].next = NULL;
                      // Adjust the tail index so clusters can start dequeuing tasks
            
                      // TODO: Add flushline?
                      TQ_INDEX_ARRAY_TAIL_UPDATE();
                    }
                    // XXX: RELEASE LOCK [[ENQUEUE]]
                    spin_unlock(&GLOBAL_tq_enqueue_lock);
            asm volatile (" #; [[ENQUEUE LOOP ]] END NON-INLINE ASM CRIT SECTION  ");
            #else
            asm volatile (" #; [[ENQUEUE LOOP ]] BEGIN INLINE ASM CRIT SECTION  ");
            // Some thoughts on the implementation started here for a fast critical section
            // for enqueue loop.  We have to pay the cost of loop overhead.  I think we can
            // get rid of this by making the minimum group size the same as the number of
            // cores in a cluster.  If we want oddball numbers we can just check
            // TQ_GROUP_SIZE & 0x07 to see if it is zero.  If zero, all is swell and we loop
            // through unrolled chuncks of eight.  If not, we pay the price and do them
            // serially.
            //
            // r20: &GLOBAL_tq_tail_index 
            // r21: &GLOBAL_taskqueue_data[0]
            // r22: &GLOBAL_taskqueue_index[0]
            // r23: &GLOBAL_taskqueue_index[GLOBAL_tq_tail_index]
            // r24: GLOBAL_tq_tail_index << 5 (offset for sizeof(TQ_TaskGroupDesc))
            // r25: &GLOBAL_taskqueue_data[tq_data_offset+XX] 
            //        FIXME: handle wraparound
            // r26: &GLOBAL_taskqueue_index[GLOBAL_tq_tail_index+task_count]
            // %0: task_count << 2 (offset for sizeof(int))
            asm volatile (
              /* Grab the tail pointer and put in (static) (r20)  */
//"mvui $28, 0xbaab\n"
              "   mvui        $20, %%hi(GLOBAL_tq_tail_index);    \n"
              "   addi        $20, $20, GLOBAL_tq_tail_index      \n"
              /* Get GLOBAL_taskqueue_data (static) (r21)         */
              "   mvui        $21, %%hi(GLOBAL_taskqueue_data);   \n"
              "   addi        $21, $21, GLOBAL_taskqueue_data;    \n"
              "   ldw         $21, $21, 0                         \n"
              /* Get GLOBAL_taskqueue_index (static) (r22)        */
              "   mvui        $22, %%hi(GLOBAL_taskqueue_index);  \n"
              "   addi        $22, $22, GLOBAL_taskqueue_index;   \n"
              "   ldw         $22, $22, 0                         \n"
              /* &GLOBAL_taskqueue_data[tq_data_offset+0] -> (r25) */
              "   add         $25, $21, %1                        \n"  
              /* spin_lock(&GLOBAL_tq_enqueue_lock);              */
              "   ori         $27, $zero, 1;                       \n"  
//"printreg  $28\n"
              "ACQ_FAILED_%=:                                     \n"  
              "   atom.xchg   $27, %2, 0                           \n"  
              // XXX START CRITICAL SECTION XXX
              "   bne         $zero, $27, ACQ_FAILED_%=;          \n"  
//"addi $28, $28, 1\n"
//"printreg  $28\n"
              /*  GLOBAL_tq_tail_index -> (r24)                   */
              "   g.ldw       $24, $20, 0;                        \n"
              /* Since we count by cachelines, must mul by 32     */
              "   slli        $24, $24, 5;                        \n"
              /* &GLOBAL_taskqueue_index[tail_idx] -> (r23)       */
              "   add         $23, $24, $22                       \n"
              "   add         $26, $23, %0                        \n"
              /* 1. Insert task pointer into TaskGroupDesc        */
              /* 2. Increment pointer to TaskGroupDesc            */
              /* 3. Check if we have exceeded task_count          */
              /* Note that 22+N in stw is the same as 23          */
              /* GLOBAL_taskqueue_index[tail_idx]                 */
              /*  task_count == 1                                 */ 
              "   stw         $25, $23, 0;                        \n" 
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 2                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 4;                        \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 3                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 8;                        \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 4                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 12;                       \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 5                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 16;                       \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 6                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 20;                       \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 7                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 24;                       \n"
              "   addi        $23, $23, 4;                        \n"
              "   beq         $23, $26, END_%=                    \n"
              /*  task_count == 8                                 */ 
              "   addi        $25, $25, 32;                       \n"  
              "   stw         $25, $22, 28;                       \n"
              "   addi        $23, $23, 4;                        \n"
              /*  task_count == 8 (Fall Through)                  */ 
              " END_%=:                                           \n"
              /* Flush out the TaskGroupDesc                      */
              "   line.wb     $22                                 \n"
              "   line.inv    $22                                 \n"
              /* Set the last *next entry (off =                  */
              /*      16 - sizeof(TQ_TaskDescEntry) ) to NULL     */
              "   g.stw         $zero, $25, 16                     \n"
              /* Update tail pointer to insert task group         */
              "   atom.inc    $20, $20, 0                         \n"
              // XXX END CRITICAL SECTION XXX
              /* spin_unlock(&GLOBAL_tq_enqueue_lock);           */
              "   atom.xchg      $zero, %2, 0                     \n"  
              : /* OUTPUT*/
              : /* INPUT */
                "r"(task_count * sizeof(unsigned int)),       // %0
                "r"(tq_offset  * sizeof(TQ_TaskDescEntry)),   // %1
                "r"(&GLOBAL_tq_enqueue_lock)
							: "20", "21", "22", "23", "24", "25", "26", "27", "28", "memory"
            );
            asm volatile (" #; [[ENQUEUE LOOP ]] END INLINE ASM CRIT SECTION  ");
            #endif
          }

          // Use the group offset in the next iteration.
          tq_group_offset += __DEFAULT_TASK_GROUP_SIZE;
// STOP ENQUEUE TASK GROUP        
//RigelPrint(0xbcda0006);    
EVENTTRACK_ENQUEUE_GROUP_END();
        }
      }
    }
    // XXX: [[LOCK RELEASED]]
    CLUSTER_gtq_lock[cluster_num].val = 0;
  }
  // For the enqueue_loop sense, if we did not have the global lock, nothing
  // needs to be done.
//PRINT_HISTOGRAM(0xccdd0002);

  return TQ_RET_SUCCESS;
}

