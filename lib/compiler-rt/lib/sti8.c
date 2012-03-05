#include <stdint.h>
void __sti8(void * const ptr, const uint8_t val)
{
	const unsigned int byteoffset = ((uintptr_t)ptr) & 0x3;
  uint32_t * const wordaddr = (uint32_t *)((uintptr_t)ptr & ~(0x3));
	const uint32_t word = *wordaddr;
	*wordaddr = (word & ~(0x000000FFU << (8*byteoffset))) | (val << (8*byteoffset));
}

