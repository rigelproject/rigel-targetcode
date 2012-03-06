#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[])
{
	FILE *in = fopen(argv[1], "rb");
  FILE *out = fopen(argv[2], "w");
	unsigned int numTriangles;
	fread(&numTriangles, sizeof(numTriangles), 1, in);
	float *v = (float *)malloc(numTriangles*9*sizeof(*v));
	assert(v && "Could not allocate vertex array");
	fread(v, sizeof(*v), numTriangles*9, in);
	for(int i = 0; i < numTriangles; i++) {
		for(int j = 0; j < 3; j++) {
			fprintf(out, "v %f %f %f\n", v[9*i+3*j+0], v[9*i+3*j+1], v[9*i+3*j+2]);
		}
	}
  for(int i = 0; i < numTriangles; i++)
  {
    fprintf(out, "f %d %d %d\n", 3*i+1, 3*i+2, 3*i+3);
  }
  fclose(out);
  return 0;
}

