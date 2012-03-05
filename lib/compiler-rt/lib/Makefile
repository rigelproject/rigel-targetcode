include ${RIGEL_TARGETCODE}/src/Makefile.common

STEMS = $(patsubst %.f,%, $(patsubst %.c,%, $(SOURCES)))
OBJS = $(patsubst %,%.ro,$(STEMS))
SOURCES = absvdi2.c absvdi2.c absvsi2.c absvti2.c adddf3.c addsf3.c addvdi3.c addvsi3.c addvti3.c \
          apple_versioning.c ashldi3.c ashlti3.c ashrdi3.c ashrti3.c clear_cache.c clzdi2.c clzsi2.c \
	  clzti2.c cmpdi2.c cmpti2.c comparedf2.c comparesf2.c ctzdi2.c ctzsi2.c ctzti2.c \
	  divdf3.c divdi3.c divsf3.c divsi3.c divti3.c extendsfdf2.c \
	  ffsdi2.c ffsti2.c fixdfdi.c fixdfsi.c fixdfti.c fixsfdi.c fixsfsi.c fixsfti.c fixunsdfdi.c \
	  fixunsdfsi.c fixunsdfti.c fixunssfdi.c fixunssfsi.c fixunssfti.c fixunsxfdi.c fixunsxfsi.c \
	  fixunsxfti.c fixxfdi.c fixxfti.c floatdidf.c floatdisf.c floatdixf.c floatsidf.c floatsisf.c \
	  floattidf.c floattisf.c floattixf.c floatundidf.c floatundisf.c floatundixf.c floatunsidf.c \
	  floatunsisf.c floatuntidf.c floatuntisf.c floatuntixf.c gcc_personality_v0.c lshrdi3.c lshrti3.c \
	  moddi3.c modsi3.c modti3.c muldf3.c muldi3.c mulsf3.c multi3.c mulvdi3.c mulvsi3.c mulvti3.c \
	  negdf2.c negdi2.c negsf2.c negti2.c negvdi2.c negvsi2.c negvti2.c paritydi2.c paritysi2.c parityti2.c \
	  popcountdi2.c popcountsi2.c popcountti2.c powidf2.c powisf2.c powitf2.c powixf2.c subvdi3.c subvsi3.c \
	  subvti3.c trampoline_setup.c truncdfsf2.c ucmpdi2.c ucmpti2.c udivdi3.c udivmoddi4.c udivmodti4.c \
	  udivsi3.c udivti3.c umoddi3.c umodsi3.c umodti3.c unalignedldi16.c unalignedldi32.c \
    unalignedsti16.c unalignedsti32.c sti8.c \
		divdc3.c divsc3.c divxc3.c muldc3.c mulsc3.c mulxc3.c

# BROKEN DUE TO MISSING <complex.h> (C99) support
#divdc3.c
#divsc3.c
#divxc3.c
#muldc3.c
#mulsc3.c
#mulxc3.c
# BROKEN DUE TO RIGEL NOT USING AN OPERATING SYSTEM
#enable_execute_stack.c
# DISABLED DUE TO NEWLIB INCLUDING ITS OWN VERSION
#eprintf.c

.PHONY: check-env

all: libcompiler_rt.a

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL is not set)
endif

install: libcompiler_rt.a check-env
	cp libcompiler_rt.a ${RIGEL_INSTALL}/target/lib/

libcompiler_rt.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

clean :
	rm -f *.ro *.o *.bc *.a *.dis *.ll cerr.dump cout.dump

%.ro: %.c
	$(CLANG) $(CLANG_CFLAGS) $(EXTRA_CFLAGS) $< -c -o $@

