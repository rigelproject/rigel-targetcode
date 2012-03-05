#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <machine/suds_primops.h>
#include <rigel.h>
#include <spinlock.h>
#include <sys/reent.h>
#include <string.h> //For memset() et al., needed to initialize reentrancy structures

// Assure mutual exclusion with flag.  It is tested upon entering a malloc
// call and cleared on exit.  Should be replaced with a spin lock later.
ATOMICFLAG_INIT_CLEAR(__init_flag);

// base address of the hybrid CC table.
uint32_t hybridCC_base_addr;

#if !defined(USER_MAIN)
#define USER_MAIN main
#endif

//Use mifmalloc by default for now.  dlmalloc should provide substantial perf gains though.
#define RIGEL_USE_DLMALLOC
#ifdef RIGEL_USE_DLMALLOC
#include <nedmalloc.h>
uintptr_t malloc_arena_start;
uintptr_t malloc_arena_end;
nedpool *incoherent_pool;
#endif

int USER_MAIN(int argc, char* argv[]);

int RigelIsSim();
//void __malloc_init(uintptr_t arena_start, uintptr_t arena_end);
//void __incoherent_malloc_init(uintptr_t arena_start_addr, uintptr_t arena_end_addr);
void *malloc(size_t bytes);
void __set_local_alloca_start_point(uint32_t);
int atexit(void(*function)(void));
int RigelIsSim(void);

extern void __rigel_premain(void) __attribute__ ((weak));
//FIXME See note in ${RIGEL_TARGETCODE}/lib/newlib/newlib/libc/sys/rigel/getreent.c about why these have to be statically (over)allocated for now.
//Also see the FIXME there.
extern struct _reent *__reent_ptrs[];
extern struct _reent __reent_data[];

void
__start(int argc, char* argv[])
{
  int tid = RigelGetThreadNum(); 
  int mainval;
  struct _reent *reent_data;
	extern char _end; //Defined by the linker script
  // NOTE: If RigelIsSim() returns false, we are running RTL or some other target
  // that doesn't have meaningful implementations of file I/O, etc., or we
  // just need to start things up really fast.
  if( RigelIsSim() ) {
    // Global state should only be touched by core zero?  We could wrap it with
    // locks or do malloc with TLS or something.
    if (tid == 0) {
        // uint32_t malloc_start = (((uint32_t)&(argv[argc])) + 19) & 0xfffffff0;
        // FIXME malloc() starts at an arbitrary address...this will probably need
        // some fixing later.  XXX: NOTE: If you change the start or the size,
        // these values need to be propagated to sim.h.  You *must* set the
        // (IN)COHERENT_MALLOC_(START|END) parameters.
			  // FIXME FIXME FIXME The simulator now has incorrect INCOHERENT_MALLOC_* data
			  //                   We need to fix it, likely with some mechanism to tell the simulator from target
			  //                   Code about a region of memory.  registerInterval("name", begin_addr, end_addr)?
#ifdef RIGEL_USE_DLMALLOC
			  malloc_arena_start = &_end;
				malloc_arena_end = 0xBFFFFFFF; //We reserve a fixed 1GB for stack at the top of the address space.  FIXME.
				incoherent_pool = nedcreatepool(128*1024*1024, -1);
#else
        uint32_t malloc_start = &_end;
        // 512MB coherent/512MB incoherent.  With no incoherent mallocs, coherent
        // malloc can take full 1 GiB.
        uint32_t malloc_size =  0x20000000;
        /* set up about 1024 megs of heap */
        //__malloc_init(malloc_start, malloc_size);
        //__incoherent_malloc_init(malloc_start+malloc_size, malloc_size);
      // Set up the table that is used for the hybrid CC scheme.  One bit per line
      // and a 4 GiB address space --> 16 MiB
      //hybridCC_base_addr = (uint32_t)malloc((16*1024*1024)*2);
      // Cache line align the base_address.
      //hybridCC_base_addr = (hybridCC_base_addr + ((1 << 24) -1)) & ~((1 << 24) -1);
      //asm volatile ( "  mtsr $13, %0;" : : "r"(hybridCC_base_addr));
#endif
      /* //NOTE This is the way we want to do it once __reents can be dynamically allocated.
      __reent_array = (struct _reent **)malloc(RigelGetNumThreads()*sizeof(*__reent_array));
      reent_data = (struct _reent *)malloc(RigelGetNumThreads()*sizeof(*reent_data));
      for(int i = 0; i < RigelGetNumThreads(); i++) {
        __reent_array[i]=&(reent_data[i]);
        _REENT_INIT_PTR(__reent_array[i]);
      }
      */

      int nthr = RigelGetNumThreads();
      for(int i = 0; i < nthr; i++) {
        __reent_ptrs[i] = &(__reent_data[i]);
        _REENT_INIT_PTR(__reent_ptrs[i]);
      }

			hybridCC_base_addr = (uint32_t)malloc((1 << 24)+32-1);
			// Cache line align the base_address.
			hybridCC_base_addr = (hybridCC_base_addr & ~(32-1));
			__asm__ __volatile__ ( "mtsr $13, %0;" : : "r"(hybridCC_base_addr));

      atomic_flag_set(&__init_flag);
    }
    else {
      atomic_flag_spin_until_set(&__init_flag);
    }
    if(__rigel_premain != NULL) {
      __rigel_premain();
    }
  }

  //Both sim and non-sim targets should run USER_MAIN().
  mainval = USER_MAIN(argc, argv);
  
  /* //NOTE These will be necessary once the reent stuff is dynamically allocated
  if(tid == 0) {
    free(__reent_array);
    free(reent_data);
  }
  */
}

void
RigelSetCoherentLine(intptr_t addr) {
  // Offset is LINESIZE_ORDER  + 3 (8 words/line) then word aligned.
  uint32_t offset = 0; 
  __asm__ ( "tbloffset %0, %1\n" : "=r"(offset) : "r"(addr));
  uint32_t hybridCC_addr = hybridCC_base_addr + offset;

  uint32_t bit_offset = (addr >> 5) & 0x1F;
  uint32_t entry = (1 << bit_offset);
  uint32_t res = 0;
  __asm__ __volatile__ ( " atom.or %0, %1, %2; "
    : "=r"(res) :"r"(entry), "r"(hybridCC_addr) : "memory");
}

void
RigelSetIncoherentLine(intptr_t addr) {
  // Offset is LINESIZE_ORDER + 2 (bits/word) + 3 (8 words/line)
  uint32_t offset = 0; 
  __asm__ ( "tbloffset %0, %1\n" : "=r"(offset) : "r"(addr));
  uint32_t hybridCC_addr = hybridCC_base_addr + offset;

  uint32_t bit_offset = (addr >> 5) & 0x1F;
  uint32_t entry = (~(1 << bit_offset));
  uint32_t res = 0;
  __asm__ __volatile__ ( " atom.and %0, %1, %2; "
    : "=r"(res) :"r"(entry), "r"(hybridCC_addr) : "memory");
}

