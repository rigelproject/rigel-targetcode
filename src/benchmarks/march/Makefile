BENCHMARK_NAME = march
SOURCES = march.c

include ../Makefile.common

convert: march_convert.c
	${CC} ${CFLAGS} -std=c99 march_convert.c -o march_convert

rp_to_obj: rp_to_obj.c
	${CC} ${CFLAGS} -std=c99 rp_to_obj.c -o rp_to_obj
