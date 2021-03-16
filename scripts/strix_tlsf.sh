#!/bin/bash

#parse command line arguments
OPTIONS=()

while [[ $# -gt 0 ]]
do
key="$1"

if [[ $key = *.tlsf ]]; then
    # tlsf file
    TLSF=$1
else
    # option
    OPTIONS+=("$1")
fi
# next argument
shift

done

if [ -z $TLSF ]; then
    echo "Error: No TLSF file given"
    exit 1
fi
if [ ! -f $TLSF ]; then
    echo "Error: File $TLSF does not exist"
    exit 1
fi

LTL=$(syfco -f ltl -q double -m fully $TLSF)
INS=$(syfco -f ltl --print-input-signals $TLSF)
OUTS=$(syfco -f ltl --print-output-signals $TLSF)

if [ -z "$INS" -a -z "$OUTS" ]; then
    cargo run --release -- -f "$LTL" ${OPTIONS[@]}
elif [ -z "$INS" ]; then
    cargo run --release -- -f "$LTL" --outs "$OUTS" ${OPTIONS[@]}
elif [ -z "$OUTS" ]; then
    cargo run --release -- -f "$LTL" --ins "$INS" ${OPTIONS[@]}
else
    cargo run --release -- -f "$LTL" --ins "$INS" --outs "$OUTS" ${OPTIONS[@]}
fi
