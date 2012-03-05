#ifndef __INCOHERENT_MALLOC_H__
#define __INCOHERENT_MALLOC_H__

void *incoherent_malloc(size_t size);
void *incoherent_calloc(size_t size, size_t len);
void *incoherent_realloc(void *old, size_t newlen);
void incoherent_free(void *ptr);

#endif //#ifndef __INCOHERENT_MALLOC_H__

