SUBDIRS= fft march stencil

all:
	@for i in $(SUBDIRS); do \
		echo "make in $$i..."; \
		(cd $$i; $(MAKE) -s clean); \
		(cd $$i; $(MAKE) -s benchmark); \
	done;

clean:
	@for i in $(SUBDIRS); do \
	echo "make clean in $$i..."; \
	(cd $$i; $(MAKE) -s clean); done

