#include <sched.h>

//On Rigel, this is a no-op
/* Yield the processor.  */
int sched_yield (void) {
	return 0;
}

