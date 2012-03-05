#include <rigel.h>
typedef union {
	  uint64_t i;
		  double f;
} fi;
void doubletest(fi x) {
	  RigelPrint((float)x.f);
}
