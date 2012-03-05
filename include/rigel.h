#ifndef _RIGEL_H_
#define _RIGEL_H_


#ifdef __cplusplus
extern "C" {
#endif

// Labels for global data and data used in inline __asm__ blocks
#define RIGEL_GLOBAL volatile
#define RIGEL_ASM volatile

// Get the includes shared with RigelSim
#include <rigellib.h>
#include <stdint.h>
#include <stdlib.h>
#include "rigel-sim.h"
#include "machine/suds_primops.h"

// Aligned data structure to mitigate the effect of false sharing
typedef struct rigel_line_aligned_t { 
  uint32_t val; 
  uint32_t pad[__CACHE_LINE_SIZE_WORDS-1]; 
} rigel_line_aligned_t __attribute__ ((aligned(32)));

typedef struct rigel_aligned_int32_t { 
  int32_t pad0[__CACHE_LINE_SIZE_WORDS]; 
  int32_t val  __attribute__ ((aligned(32)));
  int32_t pad1[__CACHE_LINE_SIZE_WORDS-1]; 
} __attribute__ ((aligned(32))) rigel_aligned_int32_t;

typedef struct rigel_aligned_f32_t { 
  int32_t pad0[__CACHE_LINE_SIZE_WORDS]; 
  float val; 
  int32_t pad[__CACHE_LINE_SIZE_WORDS-1]; 
} rigel_aligned_f32_t __attribute__ ((aligned(32)));

// ??? Is this used anywhere or is it depricated??!?!
typedef struct BarrierInfo {
	int corenum;
	int numcores;
	int local_sense;
	int *global_sense;
	int *barrier_count;
} BarrierInfo;

// for barrier supporting multithreading
typedef struct BarrierMTInfo {
	int corenum;
	//int numcores;
  int threadnum;
  int numthreads;
	int local_sense;
	int *global_sense;
	int *barrier_count;
} BarrierMTInfo;

extern int RigelIncoherentMallocEnabled();


extern int RigelGetCoreNum();
extern int RigelGetClusterNum();
extern int RigelGetThreadNum();
extern uint32_t RigelGetCycle();


// Base address of the hybrid CC table.
extern uint32_t hybridCC_base_addr;
void RigelSetCoherentLine(intptr_t addr);
void RigelSetIncoherentLine(intptr_t addr);

extern int RigelGetNumCores();
extern int RigelGetNumCoresPerCluster();
extern int RigelGetNumThreads();
extern int RigelGetNumThreadsPerCluster();
extern int RigelGetNumThreadsPerCore();

void RigelBreakpoint();

void RigelBarrier(BarrierInfo *bi);
float LocalReduceFloat(BarrierInfo *bi, float val, volatile float *ReduceVals);

// Enable/diable non-blocking atomics on a thread-by-thread basis.  The SPR for
// non-blocking atomics is $r14.
#define ENABLE_NONBLOCKING_ATOMICS() do {                     \
    __asm__ __volatile__ ( " ori  $1, $zero, 1;\n"   \
                   " mtsr $14, $1; ");        \
  } while (0);
#define DISABLE_NONBLOCKING_ATOMICS() do {                \
  __asm__ __volatile__ ( "mtsr $14, $zero; ");     \
 } while (0);

// TODO: make the rf num take the enum value?
#define SIM_SLEEP_ON() do {                     \
    int tid = RigelGetThreadNum();            \
    if (tid == 0) {                         \
      __asm__ __volatile__ ( " ori  $1, $zero, 1;\n"   \
                     " mtsr $9, $1; ");        \
    }                                           \
  } while (0);

// TODO: make the rf num take the enum value?
#define SIM_SLEEP_OFF() do {                \
  int tid = RigelGetThreadNum();          \
  if (tid == 0) {                       \
    __asm__ __volatile__ ( "mtsr $9, $zero; ");     \
  }                                         \
 } while (0);

// XXX: Fast timers using single instructions
#define StartTimer_FAST(x) do {\
	__asm__ __volatile__ ( 	"timer.start %0"\
									:\
									: "r"(x)\
								); \
	} while(0);

#define StopTimer_FAST(x) do {\
	__asm__ __volatile__ ( 	"timer.stop %0"\
									:\
									: "r"(x)\
								); \
	} while(0);

// TODO: Implement new instruction
#define ClearTimer_FAST(x) do {\
	__asm__ __volatile__ ( 	"timer.clear %0"\
									:\
									: "r"(x)\
								); \
	} while(0);

#define RigelAbort() do { asm volatile ("abort"); } while (0)

#define RigelCountLeadingZeros(leadingZeros, x) do { \
    __asm__ __volatile__ ( "clz %0, %1; \n" \
         : "=r"(leadingZeros)       \
         : "r"(x)                   \
         );                         \
        } while (0);

#define RigelGetNumClusters(num_clusters) do { \
		__asm__ __volatile__ ( "mfsr	%0, $5; \n"	 		\
				 : "=r"(num_clusters)	\
				 ); 									\
	} while (0);


#if 0
#define RigelGetClusterNum(cluster_id) do { \
		__asm__ __volatile__ ( "mfsr	%0, $4;"	 		\
				 : "=r"(cluster_id)	\
				 ); 									\
	} while (0);
#endif
// XXX: BLock prefetch instruction
// Inputs are the starting address and # of lines to fetch
#define RigelPrefetchBlock(addr, lines) do { 			\
		__asm__ __volatile__ (  "pref.b.gc %1, %0, 0 \n" \
									:									      \
									: "r"(addr), "r"(lines));				\
	} while (0); 

#define RigelPrefetchBlockCC(addr, lines) do {          \
                __asm__ __volatile__ (  "pref.b.cc %1, %0, 0 \n"\
                                :                       \
                                : "r"(addr), "r"(lines));\
        } while (0);

// XXX: GLOBAL ATOMIC DEC - Decrement ['addr'] and put the result in 'val'
#define RigelAtomicDEC(val, addr) {                                           \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);          \
    int temp;                                                                 \
	__asm__ __volatile__ (	    "ldw        %1, %2;			                       \n"\
						"atom.dec	%1, %1, 0;	                           \n"\
						"ori		%0, %1, 0;		                       \n"\
									: "=r"(val), "=r"(temp)					  \
									: "m"(_addr)							  \
									: "1", "memory");				          \
	}

// XXX: GLOBAL ATOMIC INC - Increment ['addr'] and put the result in 'val'
#define RigelAtomicINC(val, addr) { 			                              \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);          \
    int temp;                                                                 \
	__asm__ __volatile__ (	    "ldw        %1, %2;			                       \n"\
						"atom.inc	%1, %1, 0;	                           \n"\
						"ori		%0, %1, 0;		                       \n"\
									: "=r"(val), "=r"(temp)					  \
									: "m"(_addr)							  \
									: "1", "memory");				          \
	}

// XXX: GLOBAL ATOMIC EXCHANGE - Exchange 'val' for data at 'addr'
#define RigelAtomicXCHG(val, addr) {                                          \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);          \
    int temp0, temp1;                                                         \
	__asm__ __volatile__ (                          "or         %1, $0, %4;	                           \n"\
						"ldw        %2, %3;			                       \n"\
						"atom.xchg  %1, %2, 0;	                           \n"\
						"or			%0, %1, $0;	                           \n"\
						: "=r"(val), "=r"(temp0), "=r"(temp1)				  \
						: "m"(_addr), "r"(val)				                  \
						: "memory");				                  \
	} while (0); 

// XXX: GLOBAL COMPARE+SWAP: If 'compare' == ['addr'] then swapval <- ['addr']
//				and ['addr'] <- swapval

/*
										"printreg 	$22; " \
										"printreg 	$23; " \
										"printreg 	$1; " \
*/

// FIXME: This could be made a bit more streamlined
#define RigelAtomicADDU(addval, addr, retval)  { 			                  \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.addu %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}

#define RigelAtomicMAX(addval, addr, retval)  { 			                  \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.max  %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}

#define RigelAtomicMIN(addval, addr, retval)  { 			                  \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.min  %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}

#define RigelAtomicOR(addval, addr, retval)  { 			                      \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.or   %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}


#define RigelAtomicAND(addval, addr, retval)  { 			                  \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.and  %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}

#define RigelAtomicXOR(addval, addr, retval)  { 			                  \
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);	      \
    int temp0, temp1, temp2;                                                  \
	__asm__ __volatile__ ( 		"or		   %2,  $0, %5;                            \n"\
						"ldw		   %3,  %4;                            \n"\
						"atom.xor  %1,  %2, %3;                            \n"\
						"or        %0,  %1, $0;                            \n"\
						: "=r"(retval), "=r"(temp0), "=r"(temp1), "=r"(temp2) \
						: "m"(_addr), "r"(addval)		                      \
						: "memory");			                              \
	}

#define RigelAtomicCAS(compare, swapval, addr) do { 			\
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);			\
	__asm__ __volatile__ ( 		"or					$26, $0, %2; \n" \
										"or					$27, $0, %3; \n" \
										"ldw				$1, %1;			\n"	\
										"atom.cas 	$26, $27, $1;	\n"	\
										"or					%0, $26, $0;	"		\
									: "=r"(swapval)										\
									: "m"(_addr), "r"(swapval), "r"(compare)		\
									: "1", "26", "27", "memory");				\
	} while (0); 

// XXX: GLOBAL MEMORY ACCESSES - Load/Store value 'val' into the address 'addr'.



#define RigelGlobalLoadX(LocalValue, GlobalValue) do {                        \
    volatile unsigned int *_addr = (unsigned int *)(&GlobalValue);            \
    __asm__ __volatile__ ( "g.ldw       %0, %1, 0                          \n"\
                   : "=r"(LocalValue)                                         \
                   : "r"(_addr)                                               \
                   );                                                         \
    } while (0);

#define RigelGlobalStoreX(LocalValue, GlobalValue) do {                       \
    volatile unsigned int *_addr = (unsigned int *)(&GlobalValue);            \
    __asm__ __volatile__ (  "g.stw      %0, %1, 0                          \n"\
                    :                                                         \
                    : "r"(LocalValue), "r"(_addr)                             \
                    : "memory"                                                \
                    );                                                        \
    } while (0);

#define RigelGlobalLoad(val, addr) do {         \
    volatile unsigned int *_addr = (volatile unsigned int *)(&addr);            \
    __asm__ __volatile__ (      "ldw            $1, %1;         \n" \
                                        "g.ldw      $1, $1, 0; \n"      \
                                        "or             %0, $1, $0;"        \
                                    : "=r"(val)                             \
                                    : "m"(_addr)                            \
                                    : "1", "memory");               \
    } while (0); 

#define RigelGlobalStore(val, addr) do {        \
    volatile unsigned int *_addr = (volatile unsigned int *)(&addr);            \
    __asm__ __volatile__ (  "ldw        $1, %1; \n"             \
                                    "g.stw %0, $1, 0; "                 \
                                    :                                               \
                                    : "r"(val), "m"(_addr)      \
                                    : "memory");                            \
    } while (0); 



// XXX: CLUSTER-LEVEL ATOMICS  - Load-linked and store conditional.  Load-Link
// is nothing special from an API point of view.  Store-conditional has an extra
// parameter that is set to '1' when the STC succeeds and '0' should it fail
#define RigelStoreCond(val, addr, succ) do { 		\
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);			\
	__asm__ __volatile__ ( 	"ldw		$1, %2; \n"				\
									"stc 		$1, %1, $1; "			\
									"ori 		%0, $1, 0; " 			\
									: "=r"(succ)							\
									: "r"(val), "m"(_addr) 		\
									: "memory"); 							\
	} while (0); 

#define RigelLoadLinked(val, addr) do { 		\
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);			\
	__asm__ __volatile__ ( 		"ldw			$1, %1;			\n"	\
										"ldl 			$1, $1; \n"		\
										"or				%0, $1, $0;"		\
									: "=r"(val)								\
									: "m"(_addr) 							\
									: "1", "memory"); 				\
	} while (0); 

// XXX: BROADCASTS - Write a value to memory and send notifications to all of
// the cluster caches informing them of its modification
#define RigelBroadcastUpdate(val, addr) do { 		\
	volatile unsigned int *_addr = (volatile unsigned int *)(&addr);			\
	__asm__ __volatile__ ( 	"ldw		$1, %1; \n"				\
									"bcast.u %0, $1, 0; "					\
									: 												\
									: "r"(val), "m"(_addr) 		\
									: "memory"); 							\
	} while (0); 


// XXX: PIPELINE SYNC - Enforces all in-flight instructions to complete before
// 				retiring the SYNC.  When SYNC retires, fetch is also restarted.
#define RigelSync() do { \
		__asm__ ( "sync;" ); \
	} while (0);
// XXX: MEMORY BARRIER - Enforces that all previous memory operations leaving
// 				the cluster cache complete before allowing any future requests to be
// 				serviced 
#define RigelMemoryBarrier() do { \
		__asm__ ( "mb;" ); \
	} while (0);
// XXX:	WRITEBACK LINE - Flushes the line back to the global cache but does not
// 				invalidate it in the cluster cache.
// void RigelWritebackLine(void *addr)
#define RigelWritebackLine(addr) \
	do { __asm__ ( "line.wb	%0;"  	\
						:								\
						: "r"(addr)			\
						: "memory" 			\
				); 									\
		} while (0);
// XXX: INVALIDATE LINE - Invalidate the line in the cluster cache without
// 				writing it back.  If the line is not present, the request is dropped.
// void RigelInvalidateLine(void *addr)
#define RigelInvalidateLine(addr) \
	do { __asm__ ( "line.inv	%0;"  	\
						:								\
						: "r"(addr)			\
						: "memory" 			\
				); 									\
		} while (0);
// XXX:	FLUSH LINE - Flushes the line back to the global cache and invalidate it
// in the cluster cache.
// void RigelFlushLine(void *addr)
#define RigelFlushLine(addr) \
	do { __asm__ ( 	"line.wb	%0;"  	\
							"line.inv %0;"		\
						:								\
						: "r"(addr)			\
						: "memory" 			\
				); 									\
		} while (0);
// XXX: PREFETCH LINE - Brings line associated with 'addr' into the cluster
// 				cache if it is not already present
// void RigelPrefetchLine(void *addr)
#define RigelPrefetchLine(addr) \
	do { __asm__ ( "pref.l	%0, 0;"  	\
						:								\
						: "r"(addr)			\
						: "memory" 			\
				); 									\
		} while (0);
#define RigelPrefetchNoGlobalAlloc(addr) \
	do { __asm__ ( "pref.nga	%0, 0;"  	\
						:								\
						: "r"(addr)			\
						: "memory" 			\
				); 									\
		} while (0);
// void RigelPrint(uint32 x)

#define RigelPrint(x) \
    __asm__ ( "printreg %0;" : : "r"(x))

/*
//#define RigelPrint(x) \
 //   do {	asm(" addi $28, %0, 0;"\
 //       	" printreg $28;" \
 //       			: \
 //       			: "r"(x) \
 //      ); } while(0);
*/

void RigelPrintFunc(uint32_t x);

// Definition of timer libraries
int StartTimer(int timer_num);
int GetTimer(int timer_num);
int StopTimer(int timer_num);
int ClearTimer(int timer_num);

//Syscalls to seed/generate random numbers
//Seed the RNG with 64 bits of state.
//Use this to make sure subsequent runs with identical timing have identical results.
static inline void RigelSRand(unsigned int s0, unsigned int s1)
{
  __suds_syscall(0x40, s0, s1, 0);
}

//Random float between min and max (inclusive)
static inline float RigelRandFloat(const float min, const float max)
{
  uint32_t minu = *((const uint32_t *)&min);
  uint32_t maxu = *((const uint32_t *)&max);
  uint32_t ret = __suds_syscall(0x3D, minu, maxu, 0);
  return *((float *)&ret);
}

//Random (signed) int between min and max (inclusive)
static inline int RigelRandInt(const int min, const int max)
{
  uint32_t minu = *((const uint32_t *)&min);
  uint32_t maxu = *((const uint32_t *)&max);
  return (int)(__suds_syscall(0x3E, minu, maxu, 0));
}

//Random unsigned int between min and max (inclusive)
static inline unsigned int RigelRandUInt(const unsigned int min, const unsigned int max)
{
  return __suds_syscall(0x3F, min, max, 0);
}

//Returns 1 if we're running in RigelSim, 0 if otherwise (real hardware, RTL, etc.)
int RigelIsSim(void);

#ifdef __cplusplus
}
#endif

#endif
