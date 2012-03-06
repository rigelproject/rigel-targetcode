BENCHMARK_NAME = march
SOURCES = march.c

include ../Makefile.common

convert: march_convert.c
	${CC} ${CFLAGS} -std=c99 march_convert.c -o march_convert

bin_to_obj: bin_to_obj.c
	${CC} ${CFLAGS} -std=c99 rp_to_obj.c -o rp_to_obj
