.PHONY: check-env

all:

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL environment variable is undefined)
endif

install: check-env rigel.ld
	mkdir -p ${RIGEL_INSTALL}/target/lib/linker
	cp rigel.ld ${RIGEL_INSTALL}/target/lib/linker

#NOP
clean:

