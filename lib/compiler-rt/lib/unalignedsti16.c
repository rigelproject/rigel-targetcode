#include <stdint.h>
#if 0
void __unalignedsti16(void * const alignedptr, const unsigned int byteoffset, const uint16_t val)
{
	uint32_t * const lowaddr = alignedptr;
	if(byteoffset == 3) { //Crosses word boundary
    uint32_t * const highaddr = lowaddr + 1;
    *lowaddr = (*lowaddr & 0x00FFFFFFU) | (val << 24);
		*highaddr = (*highaddr & 0xFFFFFF00U) | (val >> 8);
	}
	else { //All in one word
		const uint32_t data = *lowaddr;
		*lowaddr = (data & ~(0x0000FFFFU << (8*byteoffset))) | (val << (8*byteoffset));
	}
}
#else
void __unalignedsti16(void * const unalignedptr, const uint16_t val)
{
	const unsigned int byteoffset = ((uintptr_t)unalignedptr) & 0x3;
  uint32_t * const lowaddr = (uint32_t *)((uintptr_t)unalignedptr & ~(0x3));
  if(byteoffset == 3) { //Crosses word boundary
    uint32_t * const highaddr = lowaddr + 1;
    *lowaddr = (*lowaddr & 0x00FFFFFFU) | (val << 24);
    *highaddr = (*highaddr & 0xFFFFFF00U) | (val >> 8);
  }
  else { //All in one word
    const uint32_t data = *lowaddr;
    *lowaddr = (data & ~(0x0000FFFFU << (8*byteoffset))) | (val << (8*byteoffset));
  }
}
#endif

