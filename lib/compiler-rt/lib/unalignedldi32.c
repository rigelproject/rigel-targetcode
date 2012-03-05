#include <stdint.h>

#if 0
uint32_t __unalignedldi32(const void *alignedptr, const int byteoffset)
{
	const uint32_t *lowaddr = alignedptr;
	const uint32_t *highaddr = lowaddr + 1;
	const uint32_t data = (*lowaddr >> (8*byteoffset)) | (*highaddr << (8*(4-byteoffset)));
	return data;
}
#else
uint32_t __unalignedldi32(const void *unalignedptr)
{
	  const uint32_t *lowaddr = (uint32_t *)((uintptr_t)unalignedptr & ~(0x3));
		const unsigned int byteoffset = (uintptr_t)unalignedptr & 0x3;
	  const uint32_t *highaddr = lowaddr + 1;
	  const uint32_t data = (*lowaddr >> (8*byteoffset)) | (*highaddr << (8*(4-byteoffset)));
	  return data;
}
#endif
