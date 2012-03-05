#!/usr/bin/env bash

# This script configures, builds, and installs newlib in accordance with
# the RIGEL_BUILD and RIGEL_INSTALL environment variables.

# NOTE: By default this will build binaries and libraries runnable on the current machine.
# If you want to generate 32-bit binaries/libraries on a 64-bit machine, uncomment
# the following line:

#NEWLIBHOST=i686-pc-linux-gnu

if [ -z $NEWLIBHOST ]
then
  HOSTARG=--host=${NEWLIBHOST}
else
	HOSTARG=
fi

echo "[NEWLIB] Initializing";

: ${RIGEL_BUILD?"Need to set RIGEL_BUILD"}
: ${RIGEL_INSTALL?"Need to set RIGEL_INSTALL"}
: ${RIGEL_CODEGEN?"Need to set RIGEL_CODEGEN"}

THREADS=${RIGEL_MAKE_PAR:-4}
BUILDDIR=${RIGEL_BUILD}/targetcode/lib/newlib
INSTALLDIR=${RIGEL_INSTALL}/target
SRC=${RIGEL_TARGETCODE}/lib/newlib
MAKE=${MAKE:-make}

if [ -d "${BUILDDIR}" ]
then
	echo "[NEWLIB] BUILD DIRECTORY '${BUILDDIR}' EXISTS."
	read -p "ERASE CONTENTS AND REBUILD? (y/n): " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		echo
	  rm -rf ${BUILDDIR}/*
	else
		echo
		echo "[NEWLIB] Exiting."
	fi
else
  echo "[NEWLIB] Creating build directory '${BUILDDIR}'"
  mkdir -p ${BUILDDIR}
fi

echo "[NEWLIB] Creating install directories under '${INSTALLDIR}'"
mkdir -p ${INSTALLDIR}/include
mkdir -p ${INSTALLDIR}/lib
mkdir -p ${INSTALLDIR}/rigel/include
mkdir -p ${INSTALLDIR}/rigel/lib

pushd ${BUILDDIR} >/dev/null

#FIXME Should import CFLAGS_FOR_TARGET from $RIGEL_TARGETCODE/src/Makefile.common rather than duping here

echo "[NEWLIB] Configuring in '${BUILDDIR}'"
${SRC}/configure ${HOSTARG} --prefix=${INSTALLDIR} --target=rigel --with-newlib \
CC_FOR_TARGET=${RIGEL_INSTALL}/host/bin/clang CXX_FOR_TARGET=${RIGEL_INSTALL}/host/bin/clang++ AR_FOR_TARGET=rigelar AS_FOR_TARGET=rigelas \
LD_FOR_TARGET=rigelld NM_FOR_TARGET=rigelnm RANLIB_FOR_TARGET=rigelranlib OBJDUMP_FOR_TARGET=rigelobjdump \
STRIP_FOR_TARGET=rigelstrip \
CFLAGS_FOR_TARGET="-ccc-host-triple rigel-unknown-unknown -Qunused-arguments -nostdinc -I${RIGEL_INSTALL}/host/lib/clang/2.8/include -I${RIGEL_INSTALL}/target/include -I${RIGEL_SIM}/rigel-sim/include -DLLVM28 -DRIGEL -O3 -ffast-math"
if [ $? -ne 0 ]
then
	echo "[NEWLIB] CONFIGURE FAILED"
	exit 1
fi

echo "[NEWLIB] Building source from '${SRC}' in '${BUILDDIR}'"
$MAKE -j${THREADS} all-target-libgloss
if [ $? -ne 0 ]
then
  echo "[NEWLIB] LIBGLOSS BUILD FAILED"
  exit 1
fi
$MAKE -j${THREADS} all-target-newlib
if [ $? -ne 0 ]
then
	echo "[NEWLIB] NEWLIB BUILD FAILED"
	exit 1
fi

echo "[NEWLIB] Installing from '${BUILDDIR}' to '${INSTALLDIR}'"
$MAKE -j${THREADS} install-target-libgloss
if [ $? -ne 0 ]
then
  echo "[NEWLIB] LIBGLOSS INSTALL FAILED"
  exit 1
fi
$MAKE -j${THREADS} install-target-newlib
if [ $? -ne 0 ]
then
  echo "[NEWLIB] NEWLIB INSTALL FAILED"
  exit 1
fi

echo "[NEWLIB] Moving installed files and removing temp directory at '${INSTALLDIR}/rigel'"
cp -r ${INSTALLDIR}/rigel/include/* ${INSTALLDIR}/include/
cp -r ${INSTALLDIR}/rigel/lib/* ${INSTALLDIR}/lib/
rm -rf ${INSTALLDIR}/lib/x86_64
#FIXME may need to rm ${INSTALLDIR}/lib/`uname -m` or somesuch
rm -rf ${INSTALLDIR}/rigel/include
rm -rf ${INSTALLDIR}/rigel/lib
rmdir ${INSTALLDIR}/rigel #do it this way instead of rm -rf so that, if something else
                       #happens to install something in ${INSTALLDIR}/rigel ,
											 #we will show an error instead of blowing it away.
if [ $? -ne 0 ]
then
  echo "[NEWLIB] ERROR: TRIED TO REMOVE '${INSTALLDIR}/rigel', but something besides newlib is installed there"
  exit 1
fi

echo "[NEWLIB] Success."

popd >/dev/null
