#!/usr/bin/env bash

#[[ BENCH SPECIFIC START]]
BENCHMARK=dt
if [ $# -ne "0" ] 
then
  echo "Usage: $0"
  exit 0
fi
#[[ BENCH SPECIFIC END]]

# For local runs
RIGELSIM=/scr/`whoami`/rigel/rigel-sim/src/rigelsim
# For cluster runs
# RIGELSIM=../../rigelsim
for tl in 01 02 04 08;
do
  for cl in 16; 
  do 
    for type in par; 
    do 
      #FULL_PATH=/scr/`whoami`/rigel/benchmarks/isca_bench/__RUN__/${BENCHMARK};
      BENCH_DIR=$PWD/cl${cl}-tl${tl}-${type}-timing;
      mkdir -p ${BENCH_DIR};
      cp ${BENCHMARK}.tasks ${BENCH_DIR}/${BENCHMARK}.tasks;
      cd ${BENCH_DIR} && 
      #[[ BENCH SPECIFIC START]]
      #[[ BENCH SPECIFIC END]]
      ${RIGELSIM} -tiles ${tl} -clusters ${cl} ${BENCHMARK}.tasks 2>cerr.out 1>cout.out & 
    done;
  done; 
done;
