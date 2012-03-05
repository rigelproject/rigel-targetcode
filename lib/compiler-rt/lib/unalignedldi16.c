#include <stdint.h>
#include <rigel.h>
#if 0
uint16_t __unalignedldi16(const void *alignedptr, const int byteoffset)
{
	const uint32_t *lowaddr = alignedptr;
	if(byteoffset == 3) { //Crosses word boundary
    const uint32_t *highaddr = lowaddr + 1;
		return (*lowaddr >> 24) | ((*highaddr << 8) & 0xFF00);
	}
	else { //All in one word
		return (*lowaddr >> (8*byteoffset));
	}
}
#else
uint16_t __unalignedldi16(const void *unalignedptr)
{
  const uint32_t *lowaddr = (uint32_t *)((uintptr_t)unalignedptr & ~(0x3));
	const int byteoffset = ((intptr_t)unalignedptr) & 0x3;
  if(byteoffset == 3) { //Crosses word boundary
    const uint32_t *highaddr = lowaddr + 1;
    return ((*lowaddr) >> 24) | (((*highaddr) << 8) & 0xFF00);
  }
  else { //All in one word
    return ((*lowaddr) >> (8*byteoffset));
  }
}
#endif

