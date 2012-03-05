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
BUILD=${RIGEL_BUILD}/targetcode/lib/newlib
INSTALL=${RIGEL_INSTALL}/target
SRC=${RIGEL_TARGETCODE}/lib/newlib
MAKE=${MAKE-make}

if [ ! -d "${BUILD}" ]
then
	echo "[NEWLIB] Error: Build directory '${BUILD}' does not exist"
	exit 1
fi

echo "[NEWLIB] Creating install directories under '${INSTALL}'"
mkdir -p ${INSTALL}/include
mkdir -p ${INSTALL}/lib
mkdir -p ${INSTALL}/rigel/include
mkdir -p ${INSTALL}/rigel/lib

pushd ${BUILD} >/dev/null

echo "[NEWLIB] Building source from '${SRC}' in '${BUILD}'"
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

echo "[NEWLIB] Installing from '${BUILD}' to '${INSTALL}'"
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

echo "[NEWLIB] Moving installed files and removing temp directory at '${INSTALL}/rigel'"
cp -r ${INSTALL}/rigel/include/* ${INSTALL}/include/
cp -r ${INSTALL}/rigel/lib/* ${INSTALL}/lib/
rm -rf ${INSTALL}/lib/x86_64
#FIXME may need to rm ${INSTALL}/lib/`uname -m` or somesuch
rm -rf ${INSTALL}/rigel/include
rm -rf ${INSTALL}/rigel/lib
rmdir ${INSTALL}/rigel #do it this way instead of rm -rf so that, if something else
                       #happens to install something in ${INSTALL}/rigel ,
											 #we will show an error instead of blowing it away.
if [ $? -ne 0 ]
then
  echo "[NEWLIB] ERROR: TRIED TO REMOVE '${INSTALL}/rigel', but something besides newlib is installed there"
  exit 1
fi

echo "[NEWLIB] Success."

popd >/dev/null
