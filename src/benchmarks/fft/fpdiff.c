#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

int main(int argc, char *argv[]) {
	assert(argc == 6 && "fpdiff <R> <C> <file 1> <file 2> <tolerance>");
	int R = strtol(argv[1], NULL, 10);
	int C = strtol(argv[2], NULL, 10);
	FILE *a = fopen(argv[3],"rb");
	assert(a && "Could not open first input file");
	FILE *b = fopen(argv[4],"rb");
	assert(b && "Could not open second input file");
	double tol = strtod(argv[5], NULL);
	float *fa = (float *)malloc(R*C*sizeof(*fa));
	float *fb = (float *)malloc(R*C*sizeof(*fb));
	assert(fa && fb && "Could not malloc float buffers");
	size_t na = fread(fa, sizeof(*fa), R*C, a);
	assert(na == (R*C) && "Did not read all floats from first input file");
	size_t nb = fread(fb, sizeof(*fb), R*C, b);
	assert(nb == (R*C) && "Did not read all floats from second input file");
	int i, j;
	int fail = 0;
	for(i = 0; i < R; i++) {
		for(j = 0; j < C; j++) {
			float x = fa[j+i*C];
			float y = fb[j+i*C];
			if(abs(((double)x-y)/x) > tol) {
				fprintf(stderr, "Error: Element (%d, %d) miscompares (%f vs. %f)\n", i, j, x, y);
				fail = 1;
			}
		}
	}
	if(!fail)
		fprintf(stderr, "PASS\n");
	return fail;
}
