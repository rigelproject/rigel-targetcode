include ${RIGEL_TARGETCODE}/src/Makefile.common

STEMS = $(patsubst %.f,%, $(patsubst %.c,%, $(SOURCES)))
OBJS = $(patsubst %,%.ro,$(STEMS))
SOURCES = data.c runtime.c

# BROKEN DUE TO MISSING <complex.h> (C99) support
#divdc3.c
#divsc3.c
#divxc3.c
#muldc3.c
#mulsc3.c
#mulxc3.c
# BROKEN DUE TO RIGEL NOT USING AN OPERATING SYSTEM
#enable_execute_stack.c

.PHONY: check-env

all : libBlocksRuntime.a

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL is not set)
endif

install: libBlocksRuntime.a check-env
	cp libBlocksRuntime.a ${RIGEL_INSTALL}/target/lib/

libBlocksRuntime.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

%.ro : %.S
	$(RIGELAS) $(RIGELAS_FLAGS) -o $@ $<

clean :
	rm -f *.ro *.o *.bc *.a *.dis *.ll cerr.dump cout.dump

%.ro: %.c
	$(CLANG) $(CLANG_CFLAGS) $(EXTRA_CFLAGS) $< -c -o $@

