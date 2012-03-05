#ifndef __INCOHERENT_MALLOC_H__
#define __INCOHERENT_MALLOC_H__

void *incoherent_malloc(size_t size);
void incoherent_free(void *ptr);
//TODO incoherent_calloc/realloc?

#endif //#ifndef __INCOHERENT_MALLOC_H__

