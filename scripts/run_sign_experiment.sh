#!/bin/bash

MIN_N=4
MAX_N=100
STEP=3
REP=5

PROGRAM="./build/sign.x"
OUTPUT_DIR="data/traces/sign"

if ! [ -d "${OUTPUT_DIR}" ]; then
    echo "creating \"${OUTPUT_DIR}\""
    mkdir -p "${OUTPUT_DIR}"
else
    echo "\"${OUTPUT_DIR}\" already exists"
    exit 1
fi

for (( n=MIN_N; n<=MAX_N; n+=STEP)); do
    DEST="${OUTPUT_DIR}/sign_${n}.trace"
    echo "running ${PROGRAM} for n = ${n}. Writing to ${DEST}"
    echo "${n}" > "${DEST}"
    for ((r=0; r<REP; r+=1)); do
	${PROGRAM} -n ${n} -o "${DEST}" -simple
    done
done
