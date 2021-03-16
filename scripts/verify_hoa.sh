#!/bin/bash

# verifies a machine in HOA format against a LTL specification
# uses ltl2tgba

# exit on error
set -e
# break when pipe fails
set -o pipefail

if [ "$#" -lt 5 ]; then
    echo "Usage: $0 <implementation.hoa> <formula> <ins> <outs> <REALIZABLE/UNREALIZABLE>"
    exit 1
fi

IMPLEMENTATION=$1
LTL=$2
INS=$3
OUTS=$4
REALIZABLE=$5

if [ ! -f $IMPLEMENTATION ]; then
    echo "ERROR: Implementation not found"
    exit 1
fi
if [ "$REALIZABLE" != 'REALIZABLE' -a "$REALIZABLE" != 'UNREALIZABLE' ]; then
    echo "ERROR: Invalid status: $REALIZABLE"
    exit 1
fi

BASE=$(basename $IMPLEMENTATION)

if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    # check mealy
    WORD=$(autfilt $IMPLEMENTATION --remove-ap=$(echo $OUTS | sed -e 's/ //g') | (autfilt --complement --format '%w' || true))
else
    # check moore
    WORD=$(autfilt $IMPLEMENTATION --remove-ap=$(echo $INS | sed -e 's/ //g') | (autfilt --complement --format '%w' || true))
fi
if [ -n "$WORD" ]; then
    echo "ERROR: machine is not a valid machine: $WORD"
    exit 1
fi
# check against formula
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    WORD=$(ltl2tgba --negate -f "$LTL" | (autfilt --intersect=$IMPLEMENTATION - --format '%w' || true))
else
    WORD=$(ltl2tgba -f "$LTL" | (autfilt --intersect=$IMPLEMENTATION - --format '%w' || true))
fi
if [ -n "$WORD" ]; then
    echo "ERROR: specification language is not recognized: $WORD"
    exit 1
fi

exit 0
