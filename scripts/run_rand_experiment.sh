#!/bin/bash

MIN_N=4
MAX_N=100
STEP=3
REP=2

PROGRAM="./build/rand.x"
OUTPUT_DIR="data/traces/rand_$(date +%s)"

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
    DEST="${OUTPUT_DIR}/rand_${n}.trace"
    echo "${n}" > "${DEST}"
done

for ((r=0; r<REP; r+=1)); do
    for ((n=MIN_N; n<=MAX_N; n+=STEP)); do
	DEST="${OUTPUT_DIR}/rand_${n}.trace"
	echo "running ${PROGRAM} for n = ${n}. Writing to ${DEST}"
	${PROGRAM} -n ${n} -o "${DEST}"
    done
done
