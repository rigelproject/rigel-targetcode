CC=gcc
CFLAGS=-I${RIGEL_TARGETCODE}/src/benchmarks/common -I../include -DX86_RTM -DDEBUG_DMM -DTASK_DEBUG -m32
LDFLAGS=-L../lib -lpthread -lm -m32
AR=ar
RANLIB=ranlib

all: ../lib/librtm-x86.a rtm-test.x86 dmm.x86 heat.x86 sobel.x86 mri.x86

cg.x86 : cg.o cg-util.o ../lib/librtm-x86.a
	$(CC) $(LDFLAGS) -o $@ $^ 

%.a : %.o
	$(AR) rcs $@ $^ 

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.x86: %.o ../lib/librtm-x86.a 
	$(CC) $(LDFLAGS) -o $@ $^ 

dmmtest: dmm.x86
	echo "128" | ./$< 1>stdout.out 2>stderr.out
	diff -q golden.out result.out

clean:
	rm -f *.o *.a *.x86 *.out
