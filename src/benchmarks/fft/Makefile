BENCHMARK_NAME = fft
SOURCES = ../common/descriptor_buffer_fancy.c fft.c

include ../Makefile.common

tableGen: tableGen.c
	${CC} ${CFLAGS} -lm -std=c99 -o tableGen tableGen.c

fft_host: fft_host.c
	${CC} ${CFLAGS} -O3 -lfftw3f -std=c99 -o fft_host fft_host.c

fpdiff: fpdiff.c
	${CC} ${CFLAGS} -O3 -o fpdiff fpdiff.c
