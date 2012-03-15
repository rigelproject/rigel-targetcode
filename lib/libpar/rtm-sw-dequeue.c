#include "rtm-sw.h"
#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rigellib.h"

TQ_RetType 
SW_TaskQueue_Dequeue_SLOW(int qID, TQ_TaskDesc *tdesc) 
{
  int cluster_num = -1, thread_num = 0;
  RIGEL_GLOBAL unsigned int head_idx = 0;
  TQ_RetType retval = TQ_RET_END;
  int succ = 0;

  thread_num = RigelGetThreadNum();


  // Enter the per-cluster provisional barrier
  asm volatile ( 
      "   mvui  $1, %%hi(CLUSTER_core_waitcount); \n"
      "   addi  $1, $1, CLUSTER_core_waitcount; \n"
      "   mfsr  $26, $4;                  \n"
      "   slli  $26, $26, 5; \n"
      "   add   $27, $26, $1; \n"
      "L%=_00:              \n"
      "   ldl   $26, $27; \n"
      "   addi  $26, $26, 1; \n"  // CLUSTER_core_waitcount[cluster_num]++
      "   stc   $1, $26, $27; \n"
      "   beq   $1, $zero, L%=_00;\n"
      :
    : 
    : "1", "26", "27", "memory"
  );

  while (1) {
    //TODO: What will probably be better here is a ticket lock with polling
    succ = 0;
    // Check to see if a barrier has been reached
    asm volatile (
          "   mvui  $27, %%hi(CLUSTER_percore_sense); \n"
          "   addi  $27,  $27, CLUSTER_percore_sense;   \n"
          "   mfsr  $26, $4;                          \n" // r26 has cluster_num
          "   ori   %1, $26, 0;                       \n"
          "   slli  $26, $26, 5;                      \n"
          "   add   $27, $26, $27;                    \n"   // r27 has CLUSTER_percore_sense[cluster_num] addr
          "   ldw   $28, $27, 0;                      \n"   // r28 has CLUSTER_percore_sense[cluster_num]
          "   mfsr  $26, $6;                          \n"   // r26 has thread_num
          "   mvui  $27, %%hi(LOCAL_percore_sense);   \n"
          "   addi  $27,  $27, LOCAL_percore_sense;   \n"
          "   slli  $26, $26, 2;                      \n"
          "   add   $27, $26, $27;                    \n" // r27 has addr of l_sense
          "   ldw   $1, $27, 0;                       \n" // r1 has l_sense
          "   ori   %0, $zero, 1;                     \n" // Default is succ <- 1
          "   beq   $1, $28, L%=_continue             \n"
          "   xori  $1, $1, 1                         \n"// On failure, save !l_sense to l_sense
          "   stw   $1, $27, 0                        \n" 
          "   xor   %0, %0, %0                        \n" // Not equal, succ <- 0
          "L%=_continue:                              \n" // Fall through when they do not equal
          : "=r"(succ), "=r"(cluster_num)
					:
					: "1", "26", "27", "memory"
    );
    if (!succ) { 
      return TQ_RET_SYNC; 
    }

    LOCAL_LOCK_ACQUIRE();
    // XXX: [[ACQUIRED LTQ LOCK]]
    
    // Check to see if a barrier was reached with the lock, if so return.
    if (LOCAL_percore_sense[thread_num] != CLUSTER_percore_sense[cluster_num].val) {
      LOCAL_percore_sense[thread_num] = ! LOCAL_percore_sense[thread_num];
      // XXX: [[LOCK RELEASED]]
      LOCAL_LOCK_RELEASE();

      return TQ_RET_SYNC;
    } else {
    // Start trying to dequeue tasks.  If none are available, try and get some
    // from the global queue
    if (NULL != CLUSTER_taskqueue_head[8*cluster_num]) {
      asm volatile ( 
            "   mfsr  $27, $4;    \n"     // 27 holds the cluster_num
            "   slli  $27, $27, 2; \n"
            // Shift by the BYTES_PER_LINE to remove false sharing penalty.
            "   slli  $28, $27, 3; \n"    
            "   mvui  $1, %%hi(CLUSTER_taskqueue_head); \n"
            "   addi  $1, $1, CLUSTER_taskqueue_head; \n"
            "   add   $1, $1, $28;    \n"
            "   ldw   $26, $1,  0;     \n"
            // Copy tdesc
            "   ldw   $27, $26,  0;   \n"
            "   ldw   $23, $26,  4;   \n"
            "   ldw   $24, $26,  8;   \n"
            "   ldw   $25, $26, 12;   \n"
            // Obtain and update next pointer
            "   ldw   $26, $26, 16;   \n"
            "   pref.l  $26, 0;       \n"
            "   stw   $26, $1, 0;    \n"
            // Update task decriptor that is returned to user
            "   stw   $27, %0, 0;     \n"
            "   stw   $23, %0, 4;     \n"
            "   stw   $24, %0, 8;     \n"
            "   stw   $25, %0, 12;    \n"
            // Decrement waitcount on cluster
            "   mvui  $1, %%hi(CLUSTER_core_waitcount); \n"
            "   addi  $1, $1, CLUSTER_core_waitcount; \n"
            "   add   $25, $28, $1; \n"
            "L%=_00:              \n"
            "   ldl   $26, $25; \n"
            "   subi  $26, $26, 1; \n"
            "   stc   $1, $26, $25; \n"
            "   beq   $1, $zero, L%=_00;\n"
            // Release the cluster tq lock
            "   mvui  $1, %%hi(CLUSTER_ltq_ticket_poll)\n"
            "   addi  $1, $1, CLUSTER_ltq_ticket_poll\n"
            // NOTE: Using cacheline offset into CLUSTER_ltq_ticket_poll
            "   add   $1, $1, $28\n"
            "   ldw   $27, $1, 0\n"
            "   addiu $27, $27, 1\n"
            "   stw   $27, $1, 0\n"
          :
          : "r"(tdesc)
          : "1", "23", "24", "25", "26", "27", "28", "memory"
        );
        return TQ_RET_SUCCESS;
      } else {
        RIGEL_ASM int has_global_lock = 0;

        // [[RELEASE LTQ LOCK]]
        LOCAL_LOCK_RELEASE();

        // Attempt to get the global lock for the cluster.  If we get it,
        // check for new tasks, otherwise drop the local lock and go out to
        // global task queue.  If we fail to get the lock, try again.
        asm volatile ( 
            "   mvui  $27, %%hi(CLUSTER_gtq_lock); \n"
            "   addi  $27,  $27, CLUSTER_gtq_lock;  \n"
            "   mfsr  $26, $4;                  \n"
            // GTQ is of type rigel_line_aligned_t, 32-bytes
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
          : "1", "26", "27", "memory"
        );

      if (has_global_lock) {
        TQ_TaskGroupDesc tgroup;
        int prefetch_count = 0;

        if (LOCAL_percore_sense[thread_num] != CLUSTER_percore_sense[cluster_num].val) {
          LOCAL_percore_sense[thread_num] = ! LOCAL_percore_sense[thread_num];
          // XXX: [[LOCK RELEASED]]
          CLUSTER_gtq_lock[cluster_num].val = 0;
          return TQ_RET_SYNC;
        }
        /*
         * Local task queue is empty.  Fetch more tasks/task group from global task
         * queue and insert them into the local task queue.  Return one of the tasks
         * to the requesting core.
         */
        // Check to see if someone beat us to the punch
        if (NULL == CLUSTER_taskqueue_head[cluster_num*8]) {
          while (prefetch_count++ < SWTQ_TASK_GROUP_FETCH) {
            int allow_blocking = (prefetch_count != 0);
            retval = global_swtq_cluster_dequeue(qID, &tgroup, cluster_num, allow_blocking);

            if (TQ_RET_SYNC == retval) {
              LOCAL_percore_sense[thread_num] = !LOCAL_percore_sense[thread_num];
              // [[RELEASE GTQ LOCK]]
              CLUSTER_gtq_lock[cluster_num].val = 0;
              return TQ_RET_SYNC;
            } else if (TQ_RET_BLOCK == retval) {
              // See if we were about to reach a full barrier, but we opted not to
              // wait yet, i.e., do nothing
              break;
            } else if (TQ_RET_SUCCESS == retval) {
              // Add tasks since we returned one
              TQ_TaskDescEntry *tgd = tgroup.tasks[0];
              while (tgd->next != NULL) {
                tgd = (TQ_TaskDescEntry *)tgd->next;
              }
              // Grab local task queue lock before inserting into the queue.
                LOCAL_LOCK_ACQUIRE();
                /* [[ACQUIRED LTQ LOCK]] */
                tgd->next = CLUSTER_taskqueue_head[cluster_num*8];
                CLUSTER_taskqueue_head[cluster_num*8] = tgroup.tasks[0]; 
                RigelPrefetchLine(CLUSTER_taskqueue_head[cluster_num*8]);
                /* [[RELEASE LTQ LOCK]] */
                LOCAL_LOCK_RELEASE()
                
              } else {
                RigelPrint(0xdeadbeef);
                abort();
              }
            }
          }
          // [[RELEASE GTQ LOCK]]
          CLUSTER_gtq_lock[cluster_num].val = 0;
        }
      }
    }
  }
  assert(0 && "Should have exited by here");
  return -1;
}

#define THROTTLE_DEQUEUE

//
// XXX: GLOBAL QUEUE MANAGEMENT: Grab task/task group descriptor from global
//          task queue.  Eventually this will return a task group desc instead
//          of a single desc.
//
TQ_RetType 
global_swtq_cluster_dequeue(int qID, TQ_TaskGroupDesc *tgroup, int cluster_num, 
  int allow_blocking) 
{
  RIGEL_GLOBAL unsigned int head_idx, tail_idx;

  // Code path for non-blocking global dequeues
  if (!allow_blocking) {
    int num_clusters = 0;
    RigelGlobalLoad(head_idx, GLOBAL_tq_head_index); 
    RigelGlobalLoad(tail_idx, GLOBAL_tq_head_index); 

    if (head_idx == tail_idx) {
      return TQ_RET_BLOCK;
    }
    #ifdef THROTTLE_DEQUEUE
    // FIXME: This does not deal with wraparound.  (hack :-)
    RigelGetNumClusters(num_clusters);
    if ((tail_idx >  head_idx) && ((tail_idx - head_idx) < (num_clusters*num_clusters))) {
      return TQ_RET_BLOCK;
    }
    #endif
  }

  RigelAtomicINC(head_idx, GLOBAL_tq_head_index);
  head_idx--;

try_dequeue:
  {
    unsigned int _head_idx, _tail_idx;

    // Grab the tail and check if we grabbed an entry that is valid
    RigelGlobalLoad(tail_idx, GLOBAL_tq_tail_index);

    _head_idx = head_idx;
    _tail_idx = tail_idx;
    // Check for wraparound
    if (head_idx > (tail_idx + __DEFAULT_GLOBAL_TQ_SIZE)) {
      _tail_idx = tail_idx + __DEFAULT_GLOBAL_TQ_SIZE;
      _head_idx = TQ_IDX_MOD(head_idx);
    } 

    // Entry inserted, dequeue it
    if (_head_idx < _tail_idx) {
      // Get descriptor from task queue
      *tgroup = GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx)];
      // FIXME: This is an attempt to mitigate the blowout effects we get from
      // enqueing too many tasks
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+1)])
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+3)])
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+7)])
  
      // FIXME: Insert into local queue??
      return TQ_RET_SUCCESS;
    }
  }

  // Task queue looks to be empty, enter the barrier provisionally
enter_barrier:
{ 
  // Set the flag when we enter the barrier
  int global_barrier_entry_flag = 0;
  // Flip Local Sense 
  CLUSTER_percluster_sense[cluster_num].val = !CLUSTER_percluster_sense[cluster_num].val;

  while (1) {
    unsigned int _head_idx, _tail_idx, tail_idx;
    RIGEL_GLOBAL int sense;
    RIGEL_GLOBAL int pending_local_barrier, pending_local_count;
    int junk;

    // Check the sense to see if we have already left the barrier
#ifdef USE_BCAST_UPDATE
    // BCAST will update this if it changes, otherwise just poll locally.
    sense = GLOBAL_percluster_sense;
#else
    RigelGlobalLoad(sense, GLOBAL_percluster_sense);
#endif
    if (sense == CLUSTER_percluster_sense[cluster_num].val) {
      CLUSTER_core_waitcount[cluster_num].val = 0;
      CLUSTER_percore_sense[cluster_num].val = !CLUSTER_percore_sense[cluster_num].val;
      return TQ_RET_SYNC;
    }

    RigelGlobalLoad(tail_idx, GLOBAL_tq_tail_index);

    _head_idx = head_idx;
    _tail_idx = tail_idx;
    // Check for wraparound
   if (head_idx > (tail_idx+__DEFAULT_GLOBAL_TQ_SIZE)) {
     _head_idx = TQ_IDX_MOD(head_idx);
     _tail_idx = tail_idx + __DEFAULT_GLOBAL_TQ_SIZE;
   }

   if (!global_barrier_entry_flag){
     // Grab local task queue lock
     
     LOCAL_LOCK_ACQUIRE();
     /* [[ACQUIRED LTQ LOCK]] */
     pending_local_barrier = (GLOBAL_numthreads_per_cluster ==  CLUSTER_core_waitcount[cluster_num].val);
     //pending_local_barrier = (RigelGetNumThreadsPerCluster() == CLUSTER_core_waitcount[cluster_num].val);
     /* [[RELEASE LTQ LOCK]] */
     LOCAL_LOCK_RELEASE();

     if (pending_local_barrier)  {
       int junk;
       global_barrier_entry_flag = 1;
       if (CLUSTER_percluster_sense[cluster_num].val == 0) {
         RigelAtomicINC(junk, GLOBAL_cluster_waitcount0);
       } else {
         RigelAtomicINC(junk, GLOBAL_cluster_waitcount1);
       }
     }
   }

   // The task queue is not empty!
   if (_head_idx < _tail_idx) {
     int junk;
     // Check to see if we missed a barrier occuring.  If we did, it will
     // just get caught again on the next iteration.  We could duplicate code
     // but this is a very very rare occurance.  It took me forever++ to
     // figure it out too
#ifdef USE_BCAST_UPDATE
     sense = GLOBAL_percluster_sense;
#else
     RigelGlobalLoad(sense, GLOBAL_percluster_sense);
#endif
     if (sense == CLUSTER_percluster_sense[cluster_num].val) {
       // XXX: Running to complettion, this point is never hit!
       RigelPrint(0xdeadbeef);
       continue;
     }

     if (global_barrier_entry_flag) {
       if (CLUSTER_percluster_sense[cluster_num].val == 0) {
         RigelAtomicDEC(junk, GLOBAL_cluster_waitcount0);
       } else {
         RigelAtomicDEC(junk, GLOBAL_cluster_waitcount1);
       }
     }
     // Barrier is no longer in progress.  Back out.
     CLUSTER_percluster_sense[cluster_num].val = 
       !CLUSTER_percluster_sense[cluster_num].val;

     // Get descriptor from task queue
     // FIXME: Add to local task queue
     *tgroup = GLOBAL_taskqueue_index[TQ_IDX_MOD(_head_idx)];
      // FIXME: This is an attemp to mitigate the blowout effects we get from
      // enqueing too many tasks
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+1)])
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+3)])
      RigelPrefetchLine(&GLOBAL_taskqueue_index[TQ_IDX_MOD(head_idx+7)])
     return TQ_RET_SUCCESS;

   } else if (_head_idx == (_tail_idx+GLOBAL_numclusters-1)) { 
     // Only allow this core to enter the barrier for real if it is locally
     // waiting on the barrier already.
       // Check the global wait count and if everybody else is not yet waiting,
       // skip it and try again
       int global_wc;

       if (CLUSTER_percluster_sense[cluster_num].val == 0) {
         RigelGlobalLoad(global_wc, GLOBAL_cluster_waitcount0);
       } else {
         RigelGlobalLoad(global_wc, GLOBAL_cluster_waitcount1);
       }

       if (GLOBAL_numclusters == global_wc) 
       {
         /* [[ACQUIRED LTQ LOCK]] */
         int junk;

EVENTTRACK_BARRIER_START();
//RigelPrint(0xbcda0003);    
         // This indexint is probably wrong...should probably be number of clusters
         // Actual barrier reached.  No turning back...Reset head/tail indices and
         // then notify other cores by setting sense.  
         RigelGlobalStore(tail_idx, GLOBAL_tq_head_index);

         // Reset the opposing wait counter
         if (CLUSTER_percluster_sense[cluster_num].val == 0) {
           RigelGlobalStore(0, GLOBAL_cluster_waitcount1);
         } else {
           RigelGlobalStore(0, GLOBAL_cluster_waitcount0);
         }

         // Flip the global sense.  This unleashes the GLOBAL barrier
#ifdef USE_BCAST_UPDATE
//           GLOBAL_percluster_sense = CLUSTER_percluster_sense[cluster_num];
         RigelBroadcastUpdate(CLUSTER_percluster_sense[cluster_num].val, 
           GLOBAL_percluster_sense);
#else
         RigelGlobalStore(CLUSTER_percluster_sense[cluster_num].val, 
           GLOBAL_percluster_sense);
#endif

         // These values will be visible to all the other cores as soon as
         // the lock is released.
         CLUSTER_core_waitcount[cluster_num].val = 0;
         CLUSTER_percore_sense[cluster_num].val = !CLUSTER_percore_sense[cluster_num].val;
// FIXME This is where we want to replace the polling with
// BCAST.INVALIDATE/BCAST.UPDATE if possible         

         return TQ_RET_SYNC;
      }
    }
  }
}
}

TQ_RetType 
SW_TaskQueue_Dequeue_FAST(int qID, TQ_TaskDesc *tdesc) 
{
  volatile TQ_RetType retval;
  
  // Only one of these will succeed.
  EVENTTRACK_TASK_END();
  EVENTTRACK_DEQUEUE_START();
  EVENTTRACK_IDLE_START();

  asm volatile ( 
    "   mvui  $27, %%hi(CLUSTER_ltq_ticket_count); \n"
    "   addi  $27,  $27, CLUSTER_ltq_ticket_count; \n"
    "   mvui  $24, %%hi(CLUSTER_ltq_ticket_poll);  \n"
    "   addi  $24,  $24, CLUSTER_ltq_ticket_poll;  \n"
    "   mfsr  $25, $4;                          \n"
    "   slli  $28, $25, 5;                      \n" // r28: shifted cluster_num * BYTES_PER_LINE
    "   add   $25, $27, $28;                    \n" // r25: *ticket_count
    "   add   $24, $24, $28;                    \n" // r24: *ticket_poll
    "   mfsr  $28, $4;                          \n"
    "   slli  $28, $28, 5;                      \n" // r28: shifted cluster_num 
    "   mvui  $26, %%hi(CLUSTER_taskqueue_head);\n"
    "   addi  $26,  $26, CLUSTER_taskqueue_head;\n"
    "   add   $26, $28, $26;                    \n" // r26: *HEAD, tq_head is now line-aligned
    // STEP 1. Attempt to acquire the LTQ lock
    "I%=_ACQ_LOCK:                              \n"
    "   ldl   $27, $25;                      \n" // Load the counter
    "   addiu $27, $27, 1;                      \n" // Increment the counter (RESERVE)
    "   stc   $1, $27, $25;                      \n" // Attempt to atomically swap it
    "   beq   $1, $zero, I%=_ACQ_LOCK;          \n" // STC Failed
    "   add   $25, $27, 0;                      \n" // r25: RESV value
    "L%=_poll_ticket:                           \n"
    "   ldw   $27, $24, 0;                      \n"
    "   bne   $27, $25, L%=_poll_ticket;        \n"
    // Step 2: Get the current head pointer
    "   ldw   $28, $26, 0;                      \n"
    "   bne   $28, $zero, I%=_TQ_HEAD_NOTNULL;  \n"
    // Step 3: If head of LTQ is NULL, go slow path
    "   addiu $25, $25, 1;                      \n"
    "   stw   $25, $24, 0                       \n" // Release the lock
    "   xor   $4, $4, $4                        \n" // Pass in QID (FIXME)
    "   ori   $5, %1, 0                         \n" // Pass in *tdesc
    "   stw   $ra, $sp, 24;                      \n" // XXX: Dangerous?
    "   jal   SW_TaskQueue_Dequeue_SLOW         \n" // Call slow path
    "   ldw   $ra, $sp, 24;                      \n"
    "   ori   %0, $2, 0                         \n" // Slowpath returns a TQ_RetVal
    "   jmp   I%=_DONE_DEQ                      \n" // Exit
    "I%=_TQ_HEAD_NOTNULL:                       \n"
    // Step 4: Link in the new entry and exit
    "   ldw   $27, $26,  0;                       \n" // Get HEAD
    "   ldw   $28, $27, 16;                     \n" // Get *next
    "   stw   $28, $26, 0;                      \n" // Store it into *HEAD
    "   addiu $25, $25, 1;                      \n"
    "   stw   $25, $24, 0                       \n" // Release the lock
    "   pref.l  $28, 0;                         \n" // Prefetch next task
    "   ori   %0, $zero, 0                      \n" // TQ_RetVal <- SUCCESS!
    // Step 5: Copy the structure over
    "   ldw   $1, $27, 0x0;                   \n"
    "   ldw   $28, $27, 0x4;                    \n"
    "   ldw   $26, $27, 0x8;                    \n"
    "   ldw   $25, $27, 0xc;                    \n"
    "   stw   $1,   %1, 0x0;                    \n"
    "   stw   $28,  %1, 0x4;                    \n"
    "   stw   $26,  %1, 0x8;                    \n"
    "   stw   $25,  %1, 0xc;                    \n"
    "I%=_DONE_DEQ: \n"
    : "=r"(retval)
    : "r"(tdesc)
    : "1", "2", "24", "25", "26", "27", "28", "4", "5", "memory" //TODO Is this right, since we did a call?
  );

  // The way the counters work, every valid dequeue stops the DEQ counter, 
  // starts the TASK counter, and reset the idle since one did not occur.  If
  // a SYNC is found, no dequeue happened and it should be counted as an idle.
  // No task follows either.
  if (TQ_RET_SUCCESS == retval) {
//RigelPrint(0xbcda0001);    
    EVENTTRACK_TASK_START();
    EVENTTRACK_DEQUEUE_END();
    EVENTTRACK_IDLE_RESET();
  } else {
//RigelPrint(0xbcda0002);    
    EVENTTRACK_TASK_RESET();
    EVENTTRACK_DEQUEUE_RESET();
    EVENTTRACK_IDLE_END();
    EVENTTRACK_BARRIER_END();
  }

  return retval;
}
