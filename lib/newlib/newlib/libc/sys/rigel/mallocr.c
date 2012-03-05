#include <stdlib.h>

//Reentrant versions of malloc() and friends.
//NOTE: The underlying dlmalloc/nedmalloc
//implementations are thread-safe, so no need
//to do anything with _reent here.

void *
_malloc_r (struct _reent *ptr, size_t size)
{
	  return malloc (size);
}

void *
_calloc_r (struct _reent *ptr, size_t size, size_t len)
{
	  return calloc (size, len);
}

void *
_realloc_r (struct _reent *ptr, void *old, size_t newlen)
{
	  return realloc (old, newlen);
}

void
_free_r (struct _reent *ptr, void *addr)
{
	  free (addr);
}

