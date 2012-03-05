/***************************************************************************
******************************* 05/Jan/93 **********************************
****************************   Version 1.0   *******************************
************************* Author: Paolo Cignoni ****************************
*                                                                          *
*    FILE:      main.c                                                     *
*                                                                          *
* PURPOSE:      An incremental costruction Delaunay 3d triangulator.       *
*                                                                          *
* IMPORTS:      OList                                                      *
*                                                                          *
* EXPORTS:      ProgramName to error.c                                     *
*               Statistic variables to unifgrid.c                          *
*                                                                          *
*   NOTES:      This is the optimized version of the InCoDe algorithm.     *
*               It use hashing and Uniform Grid techniques to speed up     *
*               list management and tetrahedra construction.               *
*                                                                          *
*               This program uses the 0.9 release of OList for list        *
*               management.                                                *
*                                                                          *
****************************************************************************
***************************************************************************/

#include "../common.h"

#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>



//#include <OList/general.h>
//#include <OList/error.h>
//#include <OList/olist.h>
//#include <OList/chronos.h>
/*
#include <ctype.h>
#include <math.h>
//#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include "/usr/include/time.h"
#include "graphics.h"
#include "incode.h"
#include "rigel.h"
#include "rigel-tasks.h"
  */ 

ATOMICFLAG INIT CLEAR(Init ﬂag);
const int QID = 0;
#define SW COHERENCE

#ifdef HW TQ
                  
	#define RIGEL_DEQUEUE TaskQueue_Dequeue_FAST
        #define RIGEL ENQUEUE TaskQueue Enqueue
        #define RIGEL_LOOP    TaskQueue_Loop
        #define RIGEL_INIT    TaskQueue_Init
        #define RIGEL_END     TaskQueue_End
#endif

#ifdef CLUSTER LOCAL TQ
           #define RIGEL_DEQUEUE   SW_TaskQueue_Dequeue_CLUSTER_FAST
           #define RIGEL_ENQUEUE   SW_TaskQueue_Enqueue_CLUSTER
           #define RIGEL_LOOP      SW_TaskQueue_Loop_CLUSTER_FAST
           #define RIGEL_INIT      SW_TaskQueue_Init_CLUSTER
           #define RIGEL_END       SW_TaskQueue_End_CLUSTER
#endif

#ifdef X86 RTM
     #define RIGEL_DEQUEUE    TaskQueue_Dequeue
     #define RIGEL_ENQUEUE   TaskQueue_Enqueue
     #define RIGEL_LOOP       TaskQueue_Loop
     #define RIGEL_INIT       TaskQueue_Init
     #define RIGEL_END        TaskQueue_End
#endif






/***************************************************************************
*									   *
* main									   *
*									   *
* Do the usual command line parsing, file I/O and timing matters.	   *
*									   *
***************************************************************************/



int main()
{
  
  return 0;
}
