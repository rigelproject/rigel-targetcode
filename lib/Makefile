include ${RIGEL_TARGETCODE}/src/Makefile.common

SUBDIRS=libpar taskmodel-x86 compiler-rt/lib compiler-rt/BlocksRuntime libpthread linker
SUBDIRS_PLUS_NEWLIB=$(SUBDIRS) ${RIGEL_BUILD}/targetcode/lib/newlib
ifndef MAKE
MAKE=make
endif

# Rules to finish:
# rigelpar rigel x86 merge
# TODO Refactor newlib rule, make a separate target

.PHONY: check-env

all: check-env all-no-newlib
	echo "Building newlib...";
	(cd ${RIGEL_BUILD}/targetcode/lib/newlib; $(MAKE) -s all-target-libgloss; \
	 $(MAKE) -s all-target-newlib;)

all-no-newlib: check-env
	echo "Assembling rigel-crt0-mp.S...";
	$(RIGELAS) $(RIGELAS_FLAGS) -o rigel-crt0-mp.ro rigel-crt0-mp.S
	@for i in $(SUBDIRS); do \
	echo "Building $$i..."; \
	(cd $$i; $(MAKE) -s all); done
	
install-no-newlib: check-env
	mkdir -p ${RIGEL_INSTALL}/target/include
	mkdir -p ${RIGEL_INSTALL}/target/lib
	@for i in $(SUBDIRS); do \
	echo "Installing $$i..."; \
	(cd $$i; $(MAKE) -s install); done
	echo "Installing rigel-crt0-mp.ro"
	cp rigel-crt0-mp.ro ${RIGEL_INSTALL}/target/lib/

install: check-env install-no-newlib
	echo "Installing newlib...";
	mkdir -p ${RIGEL_INSTALL}/target/include
	mkdir -p ${RIGEL_INSTALL}/target/lib
	mkdir -p ${RIGEL_INSTALL}/target/rigel/include
	mkdir -p ${RIGEL_INSTALL}/target/rigel/lib
	(cd ${RIGEL_BUILD}/targetcode/lib/newlib; $(MAKE) -s install-target-libgloss; \
	$(MAKE) -s install-target-newlib;)
	cp -r ${RIGEL_INSTALL}/target/rigel/include/* ${RIGEL_INSTALL}/target/include/
	cp -r ${RIGEL_INSTALL}/target/rigel/lib/* ${RIGEL_INSTALL}/target/lib/
	rm -rf ${RIGEL_INSTALL}/target/lib/x86_64
	#FIXME may need to rm ${RIGEL_INSTALL}/target/lib/`uname -m` or somesuch
	rm -rf ${RIGEL_INSTALL}/target/rigel/include
	rm -rf ${RIGEL_INSTALL}/target/rigel/lib
	rmdir ${RIGEL_INSTALL}/target/rigel #do it this way instead of rm -rf so that, if something else
	                                    #happens to install something in ${RIGEL_INSTALL}/target/rigel ,
	                                    #we will show an error instead of blowing it away.

#Newlib builds out-of-place, so make clean inside newlib doesn't make much sense
clean: clean-no-newlib

clean-no-newlib:
	@for i in $(SUBDIRS); do \
	echo "Cleaning $$i..."; \
	(cd $$i; $(MAKE) -s clean); done

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL is not set)
endif
ifndef RIGEL_BUILD
	$(error RIGEL_BUILD is not set)
endif
