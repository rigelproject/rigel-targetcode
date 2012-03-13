#!/usr/bin/env bash

# simple script to check output of FFT

POLICY=4
ROWS=${1:-128}
COLS=${2:-128}
CLUSTERS=${3:-1}
TILES=${4:-1}

NAME="${ROWS}x${COLS}"
TRBSIZE=8
TCBSIZE=8
PRECISION=.0001

INPUTS="$POLICY $ROWS $COLS $TRBSIZE $TCBSIZE $NAME.b"

make fft_host

./fft_host $ROWS $COLS $NAME

mv $NAME.b.out $NAME.b.out.GOLD

time echo "$INPUTS" | rigelsim -c ${CLUSTERS} -t ${TILES} fft.tasks

make fpdiff 

./fpdiff $ROWS $COLS $NAME.b.out $NAME.b.out.GOLD $PRECISION

