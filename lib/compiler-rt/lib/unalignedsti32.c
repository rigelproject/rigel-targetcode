#include <stdint.h>

#if 0
//Precondition: byteoffset == {1,2,3} (0 would not be an unaligned store, so don't call this function)
void __unalignedsti32(void * const alignedptr, const int byteoffset, const uint32_t val)
{
	uint32_t * const lowaddr = alignedptr;
	uint32_t * const highaddr = lowaddr + 1;
  
	const uint32_t lowmask = (1U << (8*byteoffset))-1;
	const uint32_t highmask = ~lowmask;
	const uint32_t lowdata = (*lowaddr & lowmask) | (val << (8*(byteoffset)));
	const uint32_t highdata = (*highaddr & highmask) | (val >> (8*(4-byteoffset)));

	*lowaddr = lowdata;
	*highaddr = highdata;
}
#else
void __unalignedsti32(void * const unalignedptr, const uint32_t val)
{
	const unsigned int byteoffset = ((uintptr_t) unalignedptr) & 0x3;
	if(byteoffset == 0) {
		*((uint32_t *) unalignedptr) = val;
	}
	else {
  	uint32_t * const lowaddr = (uint32_t *)((uintptr_t)unalignedptr & ~(0x3));
  	uint32_t * const highaddr = lowaddr + 1;
  
  	const uint32_t lowmask = (1U << (8*byteoffset))-1;
  	const uint32_t highmask = ~lowmask;
  	const uint32_t lowdata = (*lowaddr & lowmask) | (val << (8*(byteoffset)));
  	const uint32_t highdata = (*highaddr & highmask) | (val >> (8*(4-byteoffset)));

  	*lowaddr = lowdata;
  	*highaddr = highdata;
	}
}
#endif

