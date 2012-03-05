#ifndef	_SCHED_H
#define	_SCHED_H

#include <sys/cdefs.h> //For __BEGIN/END_DECLS, __THROW

__BEGIN_DECLS

//On Rigel, this is a no-op
/* Yield the processor.  */
int sched_yield (void) __THROW;

__END_DECLS

#endif /* sched.h */

