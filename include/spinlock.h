#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#define CACHE_LINE_SIZE 32
#define RIGEL_CACHE_LINE_SIZE CACHE_LINE_SIZE

typedef volatile struct __spin_lock_t {
	volatile unsigned int lock_val;
	unsigned char padding[CACHE_LINE_SIZE - sizeof(unsigned int)];
} spin_lock_t __attribute__ ((aligned (8)));

typedef volatile struct __atomic_flag_t {
	volatile unsigned int flag_val;
	unsigned char padding[CACHE_LINE_SIZE - sizeof(unsigned int)];
} atomic_flag_t __attribute__ ((aligned (8)));

//NOTE I shied away from making everything volatile because we should be interacting with these
//through the recursive_* API, which has __asm__ __volatile__'s for the relevant operations.
//Making everything volatile leads to a crapload of "discards qualifiers" warnings.
typedef struct {
	int owner;
	unsigned int count;
} recursive_spin_lock_t;

// Define a spinlock in the open state
#define SPINLOCK_INIT_OPEN(foo) \
	spin_lock_t foo __attribute__ ((section (".rigellocks"))) __attribute__ ((aligned (8)))  = { 0 };
#define SPINLOCK_INIT_CLOSED(foo) \
	spin_lock_t foo  __attribute__ ((section (".rigellocks"))) __attribute__ ((aligned (8))) = { 1 };

#define SPINLOCK_INIT_UNLOCKED(foo) SPINLOCK_INIT_OPEN(foo)
#define SPINLOCK_INIT_LOCKED(foo) SPINLOCK_INIT_CLOSED(foo)

#define RECURSIVE_SPINLOCK_INIT_OPEN(foo) \
	recursive_spin_lock_t foo __attribute__ ((section (".rigellocks"))) = { -1, 0 };
#define RECURSIVE_SPINLOCK_INIT_CLOSED(foo, owner, count) \
	recursive_spin_lock_t foo __attribute__ ((section (".rigellocks"))) = { (owner), (count) };

#define ATOMICFLAG_INIT_SET(foo) \
	atomic_flag_t foo  __attribute__ ((section (".rigellocks"))) __attribute__ ((aligned (8))) = { 1 };
#define ATOMICFLAG_INIT_CLEAR(foo) \
	atomic_flag_t foo  __attribute__ ((section (".rigellocks"))) __attribute__ ((aligned (8))) = { 0 };

void spin_lock_init(volatile spin_lock_t *s);

// XXX: SPIN_LOCK ACQUIRE (GLOBAL): Grab the spinlock.  Spin if not available.
// 				Not for highly contended locks.  Uses global operations.
// void spin_lock(volatile spin_lock_t *s) {
#if 0
#define spin_lock(s) do { \
		volatile unsigned int *lock_addr = &((s)->lock_val); \
		volatile unsigned int lock_val; 					\
		while (1) {																\
			lock_val = 1;														\
			__asm__ __volatile__ ( 	"ldw $26, %1; \n"					\
											"atom.xchg %0, $26, 0"		\
											: "=r"(lock_val) 				\
											: "m"(lock_addr) 				\
											: "1", "memory" 				\
				);																		\
			if (lock_val == 0) break;								\
		}																					\
	} while (0);
#else
#define spin_lock(s) do {		                         \
    volatile unsigned int temp;                                        \
									                 \
	__asm__ __volatile__ (  "ori            %0, $zero, 1;\n"\
					"1: " "	                      \n"\
					"atom.xchg %0, %2, 0;	      \n"\
					"bne	%0, $zero, 1b;     \n"\
					: "=r"(temp)            			         \
					: "0"(temp), "r"(s)		 		 \
				); } while(0)
#endif




// XXX: SPIN_LOCK ACQUIRE (GLOBAL): Grab the spinlock.  Spin if not available.
// 				For use with highly contended locks.  Uses global operations.
// void spin_lock_paused(volatile spin_lock_t *s) {
#define spin_lock_paused(s) do { \
		volatile unsigned int *lock_addr = &((s)->lock_val); \
		volatile unsigned int lock_val; 					\
		int pause_count = 1;											\
		while (1) {																\
			lock_val = 1;														\
			__asm__ __volatile__ ( 	"ldw $26, %1; "					\
											"atom.xchg %0, $26, 0"		\
											: "=r"(lock_val) 				\
											: "m"(lock_addr) 				\
											: "1", "memory" 				\
				);																		\
			if (lock_val == 0) break;								\
			{																				\
				int pauses = (pause_count++);					\
				while (pauses-- > 0) {								\
					__asm__ __volatile__ ( "nop;nop;nop;nop;\n" );\
				}																			\
			}																				\
		}																					\
	} while (0);


//void spin_unlock(volatile spin_lock_t *s);
#if 0
#define spin_unlock(s) do { \
	volatile unsigned int *lock_addr = &((s)->lock_val); \
	__asm__ __volatile__ ( 	"ldw $26, %0; "						\
									"atom.xchg $0, $26, 0" 		\
									:  												\
									: "m"(lock_addr) 					\
									: "1", "memory" 					\
	); } while (0);
#else
#define spin_unlock(s) do { \
	__asm__ __volatile__ (  "atom.xchg $zero, %0, 0	\n"	\
									: 													\
									: "r"(s)				 						\
				); } while (0);
#endif

int spin_lock_function(spin_lock_t *s);
int spin_lock_paused_function(spin_lock_t *s);
int spin_unlock_function(spin_lock_t *s);

void recursive_lock(recursive_spin_lock_t *s);
void recursive_unlock(recursive_spin_lock_t *s);
int recursive_trylock(recursive_spin_lock_t *s);

int atomic_flag_check(volatile atomic_flag_t *f);
void atomic_flag_set(volatile atomic_flag_t *f);
void atomic_flag_clear(volatile atomic_flag_t *f);
// Spin on flag until it clears
void atomic_flag_spin_until_clear(volatile atomic_flag_t *f);
void atomic_flag_spin_until_set(volatile atomic_flag_t *f);
		
#endif
