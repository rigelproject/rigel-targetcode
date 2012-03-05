#include <rigel-tasks.h>
#include <rigel.h>
#include <stdlib.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// barrier-mt.c
////////////////////////////////////////////////////////////////////////////////
// barrier methods updated for multiple threads

////////////////////////////////////////////////////////////////////////////////
// RigelBarrierMT_Init()
////////////////////////////////////////////////////////////////////////////////
// initialize MT compatible barrier state
void RigelBarrierMT_Init(RigelBarrierMT_Info *info)
{
  //int numcores = RigelGetNumCores();
  int numthreads = RigelGetNumThreads(); // global number of threads
  int numclusters;
  int i;

  RigelGetNumClusters(numclusters);

  info->local_complete = (rigel_line_aligned_t *)malloc(sizeof(rigel_line_aligned_t)*numclusters);
  info->global_compete = 0;
  //info->local_sense = (int *)malloc(sizeof(int)*numcores);
  info->local_sense = (int *)malloc(sizeof(int)*numthreads);
  // Set to one so we do not have to flip sense coming into first barrier
  info->global_reduce_done = 1;

  for (i = 0; i < numclusters; i++) {
    info->local_complete[i].val = 0;
    if ((i & 0x7) == 0x7) { RigelFlushLine(&(info->local_complete[i].val)); }
  }
  //for (i = 0; i < numcores; i++) {
  for (i = 0; i < numthreads; i++) {
    info->local_sense[i] = 0;
    if ((i & 0x7) == 0x7) { RigelFlushLine(&(info->local_sense[i])); }
  }
  RigelFlushLine(&(info->local_sense));
  RigelFlushLine(&(info->local_complete));
  RigelFlushLine(&(info->global_compete));
  RigelFlushLine(&(info->global_reduce_done));
}

////////////////////////////////////////////////////////////////////////////////
// void RigelBarrierMT_Destroy() 
////////////////////////////////////////////////////////////////////////////////
// destroy a barrier
void RigelBarrierMT_Destroy(RigelBarrierMT_Info *info) {
  free((void *)info->local_complete);
  info->local_complete = NULL;

  free((void *)info->local_sense);
  info->local_sense = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// void RigelBarrierMT_EnterFull()
////////////////////////////////////////////////////////////////////////////////
// enter a barrier
void RigelBarrierMT_EnterFull(RigelBarrierMT_Info *info)
{
  //int corenum = RigelGetCoreNum();
  int threadnum = RigelGetThreadNum();

  asm volatile (
    // spill
    "   addi    $sp, $sp, -16                       \n"
    "   stw     $25, $sp, 4                       \n"
    /* r28: CLUSTER_NUM                     */
    "   mfsr    $28, $4                       \n"
    // r25: THREADS_PER_CLUSTER (added for MT) 
    "   mfsr    $25, $3                       \n"
    "   addi    $25, $25, -1                  \n" // make mask for barrier
    /* Offset into local_complete[]         */
    "   slli    $27, $28, 5;                  \n"
    "   add     $27, $27, %0;                 \n"
    /* Try to atomically load local_complete */
    "BAR_localinc%=:                          \n"
    "   ldl     $26, $27;                  \n"
    "   addiu   $26, $26, 1;                  \n"
    "   stc     $1, $26, $27;                  \n"
    "   beq     $1, $zero, BAR_localinc%=;    \n"
    //"   printreg $26 \n"
    /* Is the last core to increment local count? */
    // this old code used hard-coded number of cores
    //"   andi    $26, $26, 0x0007;             \n" // this was hardcoded for 8 cores
    //"   andi    $26, $26, 0x000F;             \n" // hardcoded for 16 threads
    //"   sub     $26, $26, $25             \n" // hardcoded for 16 threads
    "   and     $26, $26, $25             \n" // mask for NUM_THREADS_PER_CLUSTER threads
    "   beq     $26, $zero, BAR_globalinc%=;  \n"
    // new code for MT (begin)
    //"   sub     $26, $26, $25;             \n" // get SPRF THREADS_PER_CLUSTERS values
    //"   printreg $26 \n"
    //"   nop \n"
    //"   be      $26, BAR_globalinc%=;          \n"
    // (end new code for MT)
    /* Otherwise we are done and should just wait */
    "   jmp     BAR_done%=;                   \n"
    "                                         \n"
    "BAR_globalinc%=:                         \n"
    "   atom.inc  $26, %1, 0                  \n"
    /* Generate mask from NUM_CLUSTERS         */
    "   mfsr      $25, $5;                    \n" // re-usd 25 here
    "   addi      $25, $25, -1                \n"
    "   and       $26, $26, $25;              \n"
    "   beq       $26, $zero, BAR_wakeup%=    \n"
    "   jmp       BAR_done%=;                 \n" 
    "                                         \n"
    /* Wakup all of the other cores by bcast'ing local_sense */
    "BAR_wakeup%=:                            \n"
#ifdef USE_BCAST_UPDATE
//    "   stw   %2, %3, 0                   \n"
    "   bcast.u   %2, %3, 0                   \n"
#else 
    // No BCAST support so revert to global polling.
    "   g.stw     %2, %3, 0                   \n"
#endif
    /* Atomically increment global_compete and wake everyone else up if equal to
     * number of clusters
     */
    "BAR_done%=:                              \n"
    // unspill
    "   ldw     $25, $sp, 4                       \n"
    "   addi    $sp, $sp, 16                       \n"
    :
    : "r"((volatile void *)info->local_complete),
      "r"(((volatile void *)&(info->global_compete))),
      "r"(info->local_sense[threadnum]),
      "r"(&info->global_reduce_done)
    : "25", "26", "27", "28"
    );

//RigelPrint(threadnum + 0x10); for debug

  // Wait until complete TODO: Just put this all into asm
#ifdef USE_BCAST_UPDATE
  while (info->local_sense[threadnum] != info->global_reduce_done);
#else
{
    int sense = 0;
    do {
      RigelGlobalLoad(sense, info->global_reduce_done);
    } while (sense != info->local_sense[threadnum]);
}
#endif

  info->local_sense[threadnum] = !info->local_sense[threadnum];
}

////////////////////////////////////////////////////////////////////////////////
// void RigelBarrierMT_EnterFull_CRef()
////////////////////////////////////////////////////////////////////////////////
// enter a barrier
void RigelBarrierMT_EnterFull_CRef(RigelBarrierMT_Info *info) {

}



