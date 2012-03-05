#ifndef __RTM_SW_H__
#define __RTM_SW_H__

#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rigellib.h"


#define PRINT_ANALYSIS_COUNTERS
//#define DEBUG_PRINT(x)
//#define DEBUG_PRINT(x) RigelPrint(x)
#define PRINT_HISTOGRAM(x) RigelPrint(x)

// FIXME: should this macro go elsewhere? 
#define EVENTTRACK_MEM_DUMP() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_MEM_DUMP)); } while (0)

#define EVENTTRACK_RF_DUMP() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RF_DUMP)); } while (0)
#define EVENTTRACK_RF_LOAD() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RF_LOAD)); } while (0)

#define EVENTTRACK_ENQUEUE_GROUP_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_GROUP_START)); } while (0)
#define EVENTTRACK_ENQUEUE_GROUP_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_GROUP_END)); } while (0)
#define EVENTTRACK_ENQUEUE_GROUP_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_GROUP_RESET)); } while (0)

#define EVENTTRACK_ENQUEUE_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_START)); } while (0)
#define EVENTTRACK_ENQUEUE_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_END)); } while (0)
#define EVENTTRACK_ENQUEUE_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_ENQUEUE_RESET)); } while (0)

#define EVENTTRACK_DEQUEUE_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_DEQUEUE_START)); } while (0)
#define EVENTTRACK_DEQUEUE_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_DEQUEUE_END)); } while (0)
#define EVENTTRACK_DEQUEUE_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_DEQUEUE_RESET)); } while (0)

#define EVENTTRACK_TASK_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_TASK_START)); } while (0)
#define EVENTTRACK_TASK_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_TASK_END)); } while (0)
#define EVENTTRACK_TASK_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_TASK_RESET)); } while (0)

#define EVENTTRACK_BARRIER_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_BARRIER_START)); } while (0)
#define EVENTTRACK_BARRIER_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_BARRIER_END)); } while (0)
#define EVENTTRACK_BARRIER_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_BARRIER_RESET)); } while (0)

#define EVENTTRACK_IDLE_START() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_IDLE_START)); } while (0)
#define EVENTTRACK_IDLE_END() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_IDLE_END)); } while (0)
#define EVENTTRACK_IDLE_RESET() \
  do { __asm__ ( "event $0, %0" : : "i"(EVT_RTM_IDLE_RESET)); } while (0)

// Note the change to add in offset for cache line aligned polling values.
/*
#define LOCAL_LOCK_ACQUIRE() do { \
    unsigned int temp1, temp2, temp3, temp4; \
    __asm__ __volatile__ ( \
    "#### BEGIN LOCAL_LOCK_ACQUIRE                                   \n"\
          "   mvui  %2, %%hi(CLUSTER_ltq_ticket_count);                    \n"\
          "   addi  %2, %2, CLUSTER_ltq_ticket_count;                      \n"\
          "   mvui  %3, %%hi(CLUSTER_ltq_ticket_poll);                     \n"\
          "   addi  %3, %3, CLUSTER_ltq_ticket_poll;                       \n"\
          "   mfsr  %0, $4;                                                \n"\
          "   slli  %0, %0, (5);                                           \n"\
          "   add   %2, %2, %0;                                            \n"\
          "   add   %3, %3, %0;                                            \n"\
          "10:                                \n"\
          "   ldl   %0, %2;                 \n"\
          "   addiu %0, %0, (1);              \n"\
          "   stc   %1, %0, %2;             \n"\
          "   beq   %1, $zero, 10b;           \n"\
          "11:                                \n"\
          "   ldw   %1, %3, (0);              \n"\
          "   bne   %1, %0, 11b;            \n"\
        : "=r"(temp1), "=r"(temp2), "=r"(temp3), "=r"(temp4) \
        : \
        : "memory" \
    ); } while(0);
*/
    #define LOCAL_LOCK_ACQUIRE() do { \
    const unsigned int clnum = RigelGetClusterNum();\
    volatile unsigned int *count = (volatile unsigned int *)&(CLUSTER_ltq_ticket_count[clnum]);\
    volatile unsigned int *poll = (volatile unsigned int *)&(CLUSTER_ltq_ticket_poll[clnum]);\
    unsigned int temp1, temp2; \
    __asm__ __volatile__ ( \
    "#### BEGIN LOCAL_LOCK_ACQUIRE                                   \n"\
    "10:                                \n"\
    "   ldl   %0, %2;                 \n"\
    "   addiu %0, %0, (1);              \n"\
    "   stc   %1, %0, %2;             \n"\
    "   beq   %1, $zero, 10b;           \n"\
    "11:                                \n"\
    "   ldw   %1, %3, (0);              \n"\
    "   bne   %1, %0, 11b;            \n"\
    : "=r"(temp1), "=r"(temp2), "+r"(count), "+r"(poll) \
    : \
    : "memory" \
    ); } while(0);

/*
    "0"(CLUSTER_ltq_ticket_count[lock_array_offset_in_words]),\
    "1"(CLUSTER_ltq_ticket_poll[lock_array_offset_in_words])\
*/
    
#define LOCAL_LOCK_ACQUIRE_() {                                                \
    int temp0, temp1, temp2;                                                  \
    __asm__ __volatile__ (                                                            \
          "#### BEGIN LOCAL_LOCK_ACQUIRE                                   \n"\
          "   mvui  %1, %%hi(CLUSTER_ltq_ticket_count);                    \n"\
          "   addi  %1, %1, CLUSTER_ltq_ticket_count;                      \n"\
          "   mvui  %2, %%hi(CLUSTER_ltq_ticket_poll);                     \n"\
          "   addi  %2, %2, CLUSTER_ltq_ticket_poll;                       \n"\
          "   mfsr  %0, $4;                                                \n"\
          "   slli  %0, %0, (5);                                           \n"\
          "   add   %1, %0, %1;                                            \n"\
          "   add   %2, %0, %2;                                            \n"\
          "                                                                \n"\
          "L%=_inc_ticket:                                                 \n"\
          "   ldl   %0, %1;                                                \n"\
          "   addiu %0, %0, 1;                                             \n"\
          "   stc   $1, %0, %1;                                            \n"\
          "   beq   $1, $zero, L%=_inc_ticket;                             \n"\
          "L%=_poll_ticket:                                                \n"\
          "   ldw   %1, %2, 0;                                             \n"\
          "   bne   %1, %0, L%=_poll_ticket;                               \n"\
        :                                                                     \
        : "r"(temp0), "r"(temp1), "r"(temp2)                                  \
    ); }

// NOTE: You are now responsible for incrementing CLUSTER_ltq_ticket_poll */

#define LOCAL_LOCK_RELEASE() do { CLUSTER_ltq_ticket_poll[cluster_num].val++; } while(0);
  

TQ_RetType 
global_swtq_cluster_dequeue(int qID, TQ_TaskGroupDesc *tdesc, 
  int cluster_num, int allow_blocking);

// Sets the default number of task groups that are fetched when a core finds the
// LTQ empty and goest to the GTQ.
extern volatile int SWTQ_TASK_GROUP_FETCH;
extern volatile int SWTQ_DATAPAR_MODE;
extern volatile int SWTQ_LOCAL_ENQUEUE_COUNT;

// Conversion routine for indices
#define TQ_IDX_MOD(idx) ((idx) % (__DEFAULT_GLOBAL_TQ_SIZE/__DEFAULT_TASK_GROUP_SIZE))
//#define TQ_DATA_MOD(idx) ((idx) % (__DEFAULT_GLOBAL_TQ_SIZE))

#define TQ_INDEX_ARRAY_TAIL_UPDATE()                                    \
  do {                                                                  \
    int __foo;                                                          \
    RigelAtomicINC(__foo, GLOBAL_tq_tail_index);                        \
  } while (0);

#define TQ_INDEX_ARRAY_HEAD_UPDATE()                                    \
  do {                                                                  \
    int __foo;                                                          \
    RigelAtomicINC(__foo, GLOBAL_tq_head_index);                        \
  } while (0);

#define TQ_INDEX_ARRAY_TAIL_GET(index)                                          \
  do {                                                                          \
    RigelGlobalLoad( index , GLOBAL_tq_tail_index);                             \
    index = index % ( __DEFAULT_GLOBAL_TQ_SIZE / __DEFAULT_TASK_GROUP_SIZE );   \
  } while (0);

#define TQ_INDEX_ARRAY_HEAD_GET(index)                                          \
  do {                                                                          \
    RigelGlobalLoad( index , GLOBAL_tq_head_index);                             \
    index = index % ( __DEFAULT_GLOBAL_TQ_SIZE / __DEFAULT_TASK_GROUP_SIZE );   \
  } while (0);

// ADDU(x, y, z) z <- x; y <- x + y;
#define TQ_DESC_POOL_GET(count, index)                                          \
  do {                                                                          \
    unsigned int __cnt = ((unsigned int) count * __DEFAULT_TASK_GROUP_SIZE);    \
    RigelAtomicADDU( __cnt, GLOBAL_tq_head_data, index);                        \
    index = (index) % ( __DEFAULT_GLOBAL_TQ_SIZE );                     \
  } while(0); 

// Modify the number of prefetches we do
int SW_TaskQueue_Set_TASK_GROUP_FETCH(int prefetch_count);
void SW_TaskQueue_Set_DATA_PARALLEL_MODE();
int SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(int enqueue_count);

extern spin_lock_t GLOBAL_tq_enqueue_lock  __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));
extern atomic_flag_t GLOBAL_tq_init_flag  __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));
extern volatile int GLOBAL_cluster_waitcount0;
extern volatile int GLOBAL_cluster_waitcount1;
extern volatile rigel_line_aligned_t CLUSTER_core_waitcount[__MAX_NUM_CLUSTERS] __attribute__ ((aligned (8))) ;;
extern int GLOBAL_numcores;
extern int GLOBAL_numclusters;
extern int GLOBAL_numthreads_per_cluster;
extern volatile unsigned int GLOBAL_numcores_SHIFT;
extern volatile unsigned int GLOBAL_numclusters_SHIFT;
extern volatile rigel_line_aligned_t CLUSTER_ltq_ticket_count[__MAX_NUM_CLUSTERS] 
 __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));
extern volatile rigel_line_aligned_t CLUSTER_ltq_ticket_poll[__MAX_NUM_CLUSTERS] 
 __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));
extern volatile rigel_line_aligned_t CLUSTER_gtq_lock[__MAX_NUM_CLUSTERS]  __attribute__ ((section (".rigellocks")));
#endif
