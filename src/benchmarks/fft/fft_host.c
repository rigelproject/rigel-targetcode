#include <fftw3.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define RANGE 100000.0f

void output(fftwf_complex *data, int M, int N, char *filename, bool binary) {
	FILE *out = fopen(filename, binary ? "wb" : "w");
	assert(out && "Could not open output file");
  for(int i = 0; i < M; i++) {
		for(int j = 0; j < N; j++) {
			if(binary)
				fwrite(data[j + i*N], sizeof(float), 2, out);
			else
			  fprintf(out, "%0.1f+%0.1fj ", data[j + i*N][0], data[j + i*N][1]);
		}
		if(!binary)
			fprintf(out, "\n");
	}
	fclose(out);
}

int main(int argc, char *argv[])
{
	assert(argc == 4 && "fft_host <M> <N> <OutputFilenamePrefix>");
  char *dummy;
  const int M = strtol(argv[1], &dummy, 10);
	const int N = strtol(argv[2], &dummy, 10);

  fftwf_complex *in = malloc(M*N*sizeof(*in));
	fftwf_complex *out = malloc(M*N*sizeof(*in));
	assert(in && out && "Failed to allocate input and output arrays");
	
	srand(time(NULL)); //Seed RNG
	for(int i = 0; i < (M*N); i++) {
		in[i][0] = (float)((float)rand()+rand()+rand()+rand()) / (float)(4.0f*RAND_MAX) * (float)RANGE;
		in[i][1] = (float)((float)rand()+rand()+rand()+rand()) / (float)(4.0f*RAND_MAX) * (float)RANGE;
	}
	
	fftwf_plan p = fftwf_plan_dft_2d(M, N, in, out, FFTW_FORWARD, FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);
  fftwf_execute(p);

	char inFilename[128], outFilename[128];
	strncpy(inFilename, argv[3], 128);
	strncpy(outFilename, argv[3], 128);
	output(in, M, N, strncat(inFilename, ".t.in", 5), false);
	output(out, M, N, strncat(outFilename, ".t.out", 6), false);
	strncpy(inFilename, argv[3], 128);
	strncpy(outFilename, argv[3], 128);
	output(in, M, N, strncat(inFilename, ".b.in", 5), true);
	output(out, M, N, strncat(outFilename, ".b.out", 6), true);
	fftwf_destroy_plan(p);
	free(out);
	free(in);
	return 0;
}

