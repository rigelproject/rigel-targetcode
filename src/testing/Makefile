#FIXME Add more subdirectories as we update them to work with this build system
SUBDIRS=assert bitreverse blocks charargs cxa_demangle fmadd malloc pthread setjmp_simple setjmp_complex printf rand rtti sincos varargs

all:
	@for i in $(SUBDIRS); do \
		echo "make in $$i..."; \
		(cd $$i; $(MAKE) -s clean); \
		(cd $$i; $(MAKE) -s benchmark); \
	done;

clean:
	echo "Removing stale results...";
	rm -rf ./__RUN__/*/*.tasks
	@for i in $(SUBDIRS); do \
	echo "make clean in $$i..."; \
	(cd $$i; $(MAKE) -s clean); done

