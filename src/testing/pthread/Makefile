BENCHMARK_NAME = pthread_test
SOURCES_C = pthread_test.c

EXTRA_LDFLAGS = -Xlinker "--whole-archive" -lpthread -Xlinker "--no-whole-archive"

include ../Makefile.common
