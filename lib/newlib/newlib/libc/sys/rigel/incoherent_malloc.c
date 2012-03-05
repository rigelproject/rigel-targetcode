#include <nedmalloc.h>

extern nedpool *incoherent_pool;

void *incoherent_malloc(size_t size)
{
	return nedpmalloc(incoherent_pool, size);
}

void *incoherent_calloc(size_t size, size_t len)
{
  return nedpcalloc(incoherent_pool, size, len);
}

void *incoherent_realloc(void *old, size_t newlen)
{
  return nedprealloc(incoherent_pool, old, newlen);
}

void incoherent_free(void *ptr)
{
	nedpfree(incoherent_pool, ptr);
}

