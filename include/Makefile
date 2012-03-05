.PHONY: check-env

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL is not set)
endif

all:

install: check-env
	mkdir -p ${RIGEL_INSTALL}/target/include
	cp -r * ${RIGEL_INSTALL}/target/include/

