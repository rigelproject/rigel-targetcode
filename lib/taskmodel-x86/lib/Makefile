CC=gcc
CFLAGS=-I../include -DX86_RTM -m32 # -DTASK_DEBUG
LDFLAGS=-lpthread -lm -m32
AR=ar
RANLIB=ranlib

.PHONY: check-env

all: librtm-x86.a 

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL is not set)
endif

install: check-env librtm-x86.a
	cp librtm-x86.a ${RIGEL_INSTALL}/host/lib/

%.a : %.o
	$(AR) rcs $@ $^ 

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.a *.x86 *.out
