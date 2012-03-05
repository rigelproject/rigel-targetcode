#!/bin/sh

THREADS=${RIGEL_MAKE_PAR:-4}
MAKE=${MAKE:-make}

$RIGEL_TARGETCODE/lib/newlib/build.sh
$MAKE -C $RIGEL_TARGETCODE/lib all-no-newlib -j${THREADS}
$MAKE -C $RIGEL_TARGETCODE/lib install-no-newlib

