#!/usr/bin/env bash

# verifies a machine in HOA format against a LTL specification
# uses ltl2tgba and autfilt

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

# check if automaton is complete with respect to the controllable actions
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    WORD=$(autfilt $IMPLEMENTATION --remove-ap=$(echo "$OUTS" | sed -e 's/ //g') | autfilt --complement --format '%w')
else
    WORD=$(autfilt $IMPLEMENTATION --remove-ap=$(echo "$INS" | sed -e 's/ //g') | autfilt --complement --format '%w')
fi
if [ -n "$WORD" ]; then
    echo "ERROR: machine is not a valid machine: $WORD"
    exit 1
fi

# check against formula
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    set +e
    WORD=$(ltl2tgba --negate -f "$LTL" | autfilt --intersect=$IMPLEMENTATION - --format '%w')
    result=$?
    set -e
else
    set +e
    WORD=$(ltl2tgba -f "$LTL" | autfilt --intersect=$IMPLEMENTATION - --format '%w')
    result=$?
    set -e
fi
if [ -n "$WORD" ]; then
    echo "ERROR: found counterexample outside of specification language: $WORD"
    exit 2
elif [ $result -ne 1 ]; then
    echo "ERROR: autfilt returned an error"
    exit 1
fi

exit 0
