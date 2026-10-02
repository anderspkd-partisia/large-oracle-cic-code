#!/bin/bash

MIN_N=4
MAX_N=100
STEP=3
REP=3

PROGRAM="./build/mul.x"
OUTPUT_DIR="data/traces/mul_$(date +%s)"

if ! [ -d "${OUTPUT_DIR}" ]; then
    echo "creating \"${OUTPUT_DIR}\""
    mkdir -p "${OUTPUT_DIR}"
else
    echo "\"${OUTPUT_DIR}\" already exists"
    exit 1
fi

# Instead of running all 5 replications for each value of n, we run all values
# of n 5 times. This (hopefully) means that we get a bit of data for all values
# of n before we have _all_ the data. I.e., it makes it faster to start
# analysing the running time data (although the data would be of a slightly
# lower quality).

for (( n=MIN_N; n<=MAX_N; n+=STEP)); do
    DEST="${OUTPUT_DIR}/mul_${n}.trace"
    echo "${n}" > "${DEST}"
done

function run {
    n=$1
    psi=$2
    dest="${OUTPUT_DIR}/mul_${n}.trace"

    echo "running ${PROGRAM} for n = ${n}, psi = ${psi}. Writing to ${dest}"
    time ${PROGRAM} -n ${n} -psi ${psi} -fake_zk -o ${dest}
}

for ((r=0; r<REP; r+=1)); do
    run 10 5
    run 15 5
    run 20 10
    run 26 13
    run 30 15
    run 34 17
    run 40 20
    run 46 23
    run 50 25
    run 56 28
    run 60 30
    run 66 33
    run 70 35
    run 76 38
    run 80 40
    run 86 43
    run 90 45
    run 96 48
    run 100 50
done

