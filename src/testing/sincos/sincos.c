#include <stdio.h>
#include <rigel.h>
#include <math.h>

int main(int argc, char *argv[]) {
	for(int i = 0; i < 10000; i++) {
		RigelPrint(sinf(RigelRandFloat(0, 2*M_PI)));
	}
	return 0;
}

