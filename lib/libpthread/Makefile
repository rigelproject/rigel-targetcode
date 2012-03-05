include ${RIGEL_TARGETCODE}/src/Makefile.common

STEMS = $(patsubst %.f,%, $(patsubst %.c,%, $(SOURCES)))
OBJS = $(patsubst %,%.ro,$(STEMS))
SOURCES = pthread_management.c pthread_tls.c pthread_once.c pthread_mutex.c pthread_cond.c

.PHONY: check-env

all: libpthread.a

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL environment variable is undefined)
endif

install: check-env libpthread.a
	cp libpthread.a ${RIGEL_INSTALL}/target/lib

libpthread.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

clean:
	rm -f *.ro *.o *.bc *.a *.dis *.ll cerr.dump cout.dump

%.ro: %.c
	$(CLANG) $(CLANG_CFLAGS) $(EXTRA_CFLAGS) $< -c -o $@
