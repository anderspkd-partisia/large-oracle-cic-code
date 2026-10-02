#!/bin/bash

# This runs the PBCTS protocol for n \in [4, ..., 32] as well as ours
# for the same number. Since the PBCTS protocol is more complete,
# compared to our proof-of-concept, we obtain a comparable running
# time for ours by running the mul and rand protocol a suitable number
# of times.
#
# We instruct the PBCTS protocol to produce 10 presignatures. In our
# protocol, that can be obtained by running mul with -m 20 and rand
# with -m 40, respectively, so that's what we do.

TOPLEVEL="data/traces/compare_$(date +%s)"

mkdir -p "${TOPLEVEL}"
mkdir -p "${TOPLEVEL}/pbcts"
mkdir -p "${TOPLEVEL}/mul"
mkdir -p "${TOPLEVEL}/rand"

echo "output directory: ${TOPLEVEL}"

function run_pbcts {
    n=$1
    dest="${TOPLEVEL}/pbcts/pbcts_${n}.trace"
    ./build/pbcts.x -n ${n} -m 10 -o ${dest}
}

function run_mul {
    n=$1
    psi=$2
    dest="${TOPLEVEL}/mul/mul_${n}.trace"
    ./build/mul.x -n ${n} -psi ${psi} -fake_zk -m 20 -o ${dest}
}

function run_rand {
    n=$1
    dest="${TOPLEVEL}/rand/rand_${n}.trace"
    ./build/rand.x -n ${n} -m 40 -o ${dest}
}

REPS=3

for ((r=0; r<REPS; r+=1)); do
    echo "running PBCTS"

    run_pbcts 4
    run_pbcts 8
    run_pbcts 12
    run_pbcts 16
    run_pbcts 20
    run_pbcts 24
    run_pbcts 28
    run_pbcts 32

    echo "running ours"
    
    ## mul
    run_mul 4 2
    run_mul 8 2
    run_mul 12 3
    run_mul 16 4
    run_mul 20 4
    run_mul 24 4
    run_mul 28 4
    run_mul 32 4

    ## rand
    run_rand 4
    run_rand 8
    run_rand 12
    run_rand 16
    run_rand 20
    run_rand 24
    run_rand 28
    run_rand 32
done
