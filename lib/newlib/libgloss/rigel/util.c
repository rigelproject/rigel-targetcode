#include "rigel.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// util.c
////////////////////////////////////////////////////////////////////////////////
// this file contains various utility API calls
// examples include returning various values (number of cores, threads,
// clustesr) stored in the special purpose register file
////////////////////////////////////////////////////////////////////////////////

// SPRF enum values:
// see the rigel-sim includes directory rigellib.h for a listing of special
// purpose register assignments!

// void RigelPrint(uint32 x)
void RigelPrintFunc(uint32_t x) {
  do {  
    asm(" addi $28, %0, 0;\n "\
        " printreg $28;   \n" \
        : \
        : "r"(x) \
    ); } while(0);
}
////////////////////////////////////////////////////////////////////////////////
// RigelDoIinit()
////////////////////////////////////////////////////////////////////////////////
// returns the caller information regarding whether init code should be done
// Used for SIM/RTL interaction, the sim will generate a inititalized memory 
// image and RTL will only execute code. Makes sense for cleaner RTL results
////////////////////////////////////////////////////////////////////////////////
int RigelIsSim(void) {
  int x;

  asm volatile ( "mfsr	%0, $16;"
                 : "=r"(x)
                );
  return x;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// RigelGetClusterNum()
////////////////////////////////////////////////////////////////////////////////
// returns the calling core's cluster number
////////////////////////////////////////////////////////////////////////////////
int RigelIncoherentMallocEnabled() {
  int x;

  asm volatile ( "mfsr	%0, $12;"
                 : "=r"(x)
                );
  return x;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// RigelGetClusterNum()
////////////////////////////////////////////////////////////////////////////////
// returns the calling core's cluster number
////////////////////////////////////////////////////////////////////////////////
int RigelGetClusterNum() {
  int cluster_id;

  asm volatile ( "mfsr	%0, $4;"
                 : "=r"(cluster_id)
                );
  return cluster_id;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// RigelGetCoreNum()
////////////////////////////////////////////////////////////////////////////////
// return the calling core's core number
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetCoreNum() {
  volatile int core;

  asm( "mfsr  %0, $0" : "=r"(core));

  return core;
}

////////////////////////////////////////////////////////////////////////////////
// RigelGetNumCores()
////////////////////////////////////////////////////////////////////////////////
// return the total number of cores in the system
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetNumCores() {
  volatile int cores;

  asm( "mfsr  %0, $1;" : "=r"(cores)
       :
       );

  return cores;
}

////////////////////////////////////////////////////////////////////////////////
// RigelGetNumCoresPerCluster()
////////////////////////////////////////////////////////////////////////////////
// return the number of cores per cluster
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetNumCoresPerCluster() {
  volatile unsigned int val;

  asm( "mfsr  %0, $2;" : "=r"(val)
       :
       );

  return val;
}

////////////////////////////////////////////////////////////////////////////////
// RigelGetThreadNum()
////////////////////////////////////////////////////////////////////////////////
// return the calling core's current thread number
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetThreadNum() {
  volatile int thread;

  asm( "mfsr  %0, $6" : "=r"(thread)
    :
  );

  return thread;
}

////////////////////////////////////////////////////////////////////////////////
// RigeGetNumThreads()
////////////////////////////////////////////////////////////////////////////////
// return the total number of threads in the system
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetNumThreads() {
  volatile int threads;

  asm( "mfsr  %0, $7" : "=r"(threads)
    :
  );

  return threads;
}

////////////////////////////////////////////////////////////////////////////////
// RigelGetNumThreadsPerCore()
////////////////////////////////////////////////////////////////////////////////
// return the number of threads per core
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetNumThreadsPerCore() {
  volatile int threads;

  asm( "mfsr  %0, $8" : "=r"(threads)
    :
  );

  return threads;
}

////////////////////////////////////////////////////////////////////////////////
// RigelGetNumThreadsPerCluster()
////////////////////////////////////////////////////////////////////////////////
// return the number of threads per cluster
// see rigellib.h in rigel-sim/include for SPRF enum values
int RigelGetNumThreadsPerCluster() {
  volatile int threads;

  asm( "mfsr  %0, $3" : "=r"(threads)
    :
  );

  return threads;
}

// TODO: if this is #if-ed out, WTF is the REAL slim shady (rigel-print)?
#if 0
void RigelPrint(unsigned int val) {
    volatile  int dummyvar;
    asm("  addi  $48, $63, 0;\n"
        " addi $63, %0, 0;\n "
        " printreg;        \n" 
        " addi $63, $48, 0;"    // r48 is live in so we can use it without worry
        :
        : "r"(val)
       );
}
#endif

////////////////////////////////////////////////////////////////////////////////
// RigelBreakpoint()
////////////////////////////////////////////////////////////////////////////////
// TODO: is this used anymore?
void RigelBreakpoint() {
  asm( "brk" );
}

////////////////////////////////////////////////////////////////////////////////
/// RigelGetCycle()
////////////////////////////////////////////////////////////////////////////////
//TODO Technically, this is just the low 32 bits of
//the cycle counter, but close enough.  If we have to
//run >4 billion cycles, we have bigger problems.
//Also, note that the __asm__ is not __volatile__,
//since I don't really care if we get a slightly stale
//or eager value.
uint32_t RigelGetCycle() {
	uint32_t ret;
  __asm__ ( " mfsr  %0, $10;" \ 
          : "=r"(ret)           \
				  );
	return ret;
}

#if 0
int
RigelAtomicInc(volatile int *addr) {
  int retval;

  asm ( "ldw $2, %1; "      // Load the address int a reg
        "atom.inc %0, $2, 0"     // (addr)++ 
        : "=r"(retval) : "m"(addr) : "2" );
  return retval;
}
#endif


////////////////////////////////////////////////////////////////////////////////
// RigelBarrier() - this be fuct?
////////////////////////////////////////////////////////////////////////////////
// TODO: is this used, or depricated? delete me later if I am not used
void RigelBarrier(BarrierInfo *bi) {
  volatile int count;

  if (bi->local_sense == 1) bi->local_sense = 0;
  else bi->local_sense = 1;

  RigelAtomicINC(count, bi->barrier_count);

  if (count == bi->numcores) { 
    *(bi->barrier_count) = 0; 
    *(bi->global_sense) = bi->local_sense;
  } else {
    while(bi->local_sense != *(bi->global_sense));
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocalReduceFloat()
////////////////////////////////////////////////////////////////////////////////
// TODO: is this used? uses the possibly depricated Barrier, hmm
// Forms two-sided barrier
float
LocalReduceFloat(BarrierInfo *bi, float val, volatile float *ReduceVals) {
  int j;

  ReduceVals[bi->corenum] = val;
  RigelBarrier(bi);
  if (bi->corenum == 0) {
    for (j = 1; j < bi->numcores; j++) ReduceVals[0] += ReduceVals[j];
  }
  RigelBarrier(bi);
  return ReduceVals[0];
}

#ifdef __cplusplus
}
#endif
