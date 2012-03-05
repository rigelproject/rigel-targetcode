#include <unistd.h>
//FIXME This question doesn't make sense if you don't have a TLB.
int getpagesize()
{
	return 65536;
}
