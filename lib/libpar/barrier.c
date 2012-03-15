#include <rigel-tasks.h>
#include <rigel.h>
#include <stdlib.h>
#include <assert.h>

void RigelBarrier_Init(RigelBarrier_Info *info)
{
  int numcores = RigelGetNumCores();
  int numclusters;
  int i;

  RigelGetNumClusters(numclusters);

  info->local_complete = (rigel_line_aligned_t *)malloc(sizeof(rigel_line_aligned_t)*numclusters);
  info->global_compete = 0;
  info->local_sense = (int *)malloc(sizeof(int)*numcores);
  // Set to one so we do not have to flip sense coming into first barrier
  info->global_reduce_done = 1;

  for (i = 0; i < numclusters; i++) {
    info->local_complete[i].val = 0;
  }
  for (i = 0; i < numcores; i++) {
    info->local_sense[i] = 0;
  }
}

void RigelBarrier_Destroy(RigelBarrier_Info *info) {
  free((void *)info->local_complete);
  info->local_complete = NULL;

  free((void *)info->local_sense);
  info->local_sense = NULL;
}

void RigelBarrier_EnterFull(RigelBarrier_Info *info)
{
  int corenum = RigelGetCoreNum();

  asm volatile (
    /* r28: CLUSTER_NUM                     */
    "   mfsr    $28, $4                       \n"
    /* Offset into local_complete[]         */
    // Offset is now full cache line.
    "   slli    $27, $28, 5;                  \n"
    "   add     $27, $27, %0;                 \n"
    /* Try to atomically load local_complete */
    "BAR_localinc%=:                          \n"
    "   ldl     $26, $27;                  \n"
    "   addiu   $26, $26, 1;                  \n"
    "   stc     $1, $26, $27;                  \n"
    "   beq     $1, $zero, BAR_localinc%=;    \n"
    /* Is the last core to increment local count? */
    "   andi    $26, $26, 0x0007;             \n"
    "   beq     $26, $zero, BAR_globalinc%=;  \n"
    /* Otherwise we are done and should just wait */
    "   jmp     BAR_done%=;                   \n"
    "                                         \n"
    "BAR_globalinc%=:                         \n"
    "   atom.inc  $26, %1, 0                  \n"
    /* Generate mask from NUM_CLUSTERS         */
    "   mfsr      $25, $5;                    \n"
    "   addi      $25, $25, -1                \n"
    "   and       $26, $26, $25;              \n"
    "   beq       $26, $zero, BAR_wakeup%=    \n"
    "   jmp       BAR_done%=;                 \n" 
    "                                         \n"
    /* Wakup all of the other cores by bcast'ing local_sense */
    "BAR_wakeup%=:                            \n"
#ifdef USE_BCAST_UPDATE
    "   bcast.u   %2, %3, 0                   \n"
//    "   stw   %2, %3, 0                   \n"
#else 
    // No BCAST support so revert to global polling.
    "   g.stw     %2, %3, 0                   \n"
#endif
    /* Atomically increment global_compete and wake everyone else up if equal to
     * number of clusters
     */
    "BAR_done%=:                              \n"
    :
    : "r"((volatile void *)info->local_complete),
      "r"(((volatile void *)&(info->global_compete))),
      "r"(info->local_sense[corenum]),
      "r"(&info->global_reduce_done)
    : "1", "25", "26", "27", "28"
    );
  // Wait until complete TODO: Just put this all into asm
#ifdef USE_BCAST_UPDATE
  while (info->local_sense[corenum] != info->global_reduce_done);
#else
{
    int sense = 0;
    do {
      RigelGlobalLoad(sense, info->global_reduce_done);
    } while (sense != info->local_sense[corenum]);
}
#endif

  info->local_sense[corenum] = !info->local_sense[corenum];
}

