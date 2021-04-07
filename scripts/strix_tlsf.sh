#!/usr/bin/env bash

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

INS=$(syfco -f ltl --print-input-signals $TLSF)
OUTS=$(syfco -f ltl --print-output-signals $TLSF)
LTL=$(syfco -f ltl -q double -m fully $TLSF)

strix --ins "$INS" --outs "$OUTS" -f "$LTL" ${OPTIONS[@]}
