#!/usr/bin/env bash

MAKE=${MAKE:-make}

# this can be replaced with a Makefile, if needed
pushd include && $MAKE install && popd
pushd lib && ./build.sh && popd
