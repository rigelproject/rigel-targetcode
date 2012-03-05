#include <rigel.h>
#include <stdio.h>
#include <stdint.h>

typedef union {
	uint64_t i;
	double f;
} fi;

void doubletest(fi x);

int main(int argc, char *argv[]) {
	if(RigelGetThreadNum() == 0) {
		double d = 1357.1818;
		char *c = (char *)&d;
		for(int i = 0; i < 8; i++, c++) {
			RigelPrint(*c);
		}
    printf("1357.1818 = %2.2f\n", 1357.1818);
		fi x;
		x.f = (double)argc;
		RigelPrint(x.i & 0xFFFFFFFFU);
		RigelPrint((x.i >> 32) & 0xFFFFFFFFU);
		doubletest(x);
		SIM_SLEEP_OFF();
	}
	return 0;
}

