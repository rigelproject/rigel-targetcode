#ifndef __RIGEL_MEMOPS_H__
#define __RIGEL_MEMOPS_H__

/* TODO TODO TODO
void RigelMemoryBarrier();
void RigelFlushLine(void *addr)
void RigelWritebackLine(void *addr);
void RigelPrefetchLine(void *addr);
*/

#define RigelMemoryBarrier() do { } while (0);
#define RigelFlushLine(x) do { } while (0);
#define RigelWritebackLine(x) do { } while (0);
#define RigelPrefetchLine(x) do {} while (0);

void RigelAtomicINC(void *addr);

#endif
