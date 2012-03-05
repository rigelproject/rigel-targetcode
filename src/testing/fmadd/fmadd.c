#include <stdio.h>
#include <stdlib.h>

float fmadd(const float a, const float b, const float c) {
	return a*b+c;
}

int main(int argc, char *argv[]) {
	float a = strtof(argv[1], NULL);
	float b = strtof(argv[2], NULL);
	float c = strtof(argv[3], NULL);
	fprintf(stderr, "result is %f\n", fmadd(a,b,c));
	return 0;
}
