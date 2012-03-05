CC=gcc
CFLAGS=-I./include -I${RIGEL_TARGETCODE}/src/benchmarks/common -DTASK_DEBUG -DX86_RTM -m32
LDFLAGS=-lpthread -lm -m32
AR=ar
RANLIB=ranlib

SUBDIRS=lib tests

all: 
	@for i in $(SUBDIRS); do \
	echo "Building $$i"; \
	(cd $$i; $(MAKE) -s all); done

clean:
	@for i in $(SUBDIRS); do \
	echo "Cleaning $$i"; \
	(cd $$i; $(MAKE) -s clean); done

install:
	(cd lib; $(MAKE) -s install);
