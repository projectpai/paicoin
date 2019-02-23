#!/bin/bash

GEN_DIFF=0x207fffff

if [ "$#" -gt 1 ]
then
    echo "Usage: build-confdiff.sh [<genesis-block-nbits>]"
    echo "       The optional <genesis-block-nbits> is given in the compact format, e.g. 0x207fffff."
    exit -1
fi

if [ "$#" -eq 1 ]
then
    GEN_DIFF=$1
    echo "Using difficulty: $GEN_DIFF"
fi

docker build --rm -t paicoin-confdiff -f Dockerfile.confdiff .

docker run --rm -i -t -e GENESIS_DIFF=$GEN_DIFF -p 8566:8566 paicoin-confdiff
