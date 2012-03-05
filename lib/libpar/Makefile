include ${RIGEL_TARGETCODE}/src/Makefile.common

STEMS = $(patsubst %.f,%, $(patsubst %.c,%, $(SOURCES)))
OBJS = $(patsubst %,%.ro,$(STEMS))
SOURCES = barrier.c hw-tasks.c rtm-sw-common.c rtm-sw-dequeue.c rtm-sw-enqueue.c rtm-sw-init.c barrier-mt.c

# Uncomment the next line to enable use of bcast.u instead of polling using
# global loads and stores.
ENABLE_BCAST=-DUSE_BCAST_UPDATE

override EXTRA_CFLAGS += ${ENABLE_BCAST}

all : libpar.a

.PHONY: check-env

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL environment variable is undefined)
endif

install: check-env libpar.a
	cp libpar.a ${RIGEL_INSTALL}/target/lib

libpar.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

clean:
	rm -f *.ro *.o *.bc *.a *.dis *.ll cerr.dump cout.dump

%.ro: %.c
	$(CLANG) $(CLANG_CFLAGS) $(EXTRA_CFLAGS) $< -c -o $@
