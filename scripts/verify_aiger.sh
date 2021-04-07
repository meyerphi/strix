#!/usr/bin/env bash

# verifies an aiger circuit against a LTL specification
# uses ltlfilt and nuXmv

# exit on error
set -e
# break when pipe fails
set -o pipefail

if [ "$#" -lt 5 ]; then
    echo "Usage: $0 <implementation.aag> <formula> <ins> <outs> <REALIZABLE/UNREALIZABLE>"
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

# verify if inputs and outputs match
if [ "$REALIZABLE" == 'UNREALIZABLE' ]; then
    tmp=$INS
    INS=$OUTS
    OUTS=$tmp
fi
if ! diff -B -q <(echo "$INS" | sed -e 's/\s*,\s*/\n/g' | sort) <(grep '^i[0-9]* ' $IMPLEMENTATION | sed -e 's/^i[0-9]* //' | sort) >/dev/null; then
    echo "ERROR: Inputs don't match: $(echo $INS | sed -e 's/\s*,\s*/\n/g' | sort) vs $(grep '^i[0-9]* ' $IMPLEMENTATION | sed -e 's/^i[0-9]* //' | sort)"
    exit 1
fi
if ! diff -B -q <(echo "$OUTS" | sed -e 's/\s*,\s*/\n/g' | sort) <(grep '^o[0-9]* ' $IMPLEMENTATION | sed -e 's/^o[0-9]* //' | sort) >/dev/null; then
    echo "ERROR: Outputs don't match"
    exit 1
fi

# model check implementation against formula
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    LTL_NORMAL=$(ltlfilt --unabbreviate=WMR -p -f "$LTL" )
else
    LTL_NORMAL=$(ltlfilt --unabbreviate=WMR --negate -p -f "$LTL")
fi
LTL_NORMAL=$(echo "$LTL_NORMAL" | sed -e 's/\<1\>/TRUE/g' -e 's/\<0\>/FALSE/g')
set +e
RESULT=$(echo "read_aiger_model -i ${IMPLEMENTATION}; encode_variables; build_boolean_model; check_ltlspec_ic3 -p \"$LTL_NORMAL\"; quit" | nuXmv -int)
result=$?
set -e

# check result
if [ $result -eq 0 ]; then
    if echo "$RESULT" | grep -q 'specification .* is true'; then
        exit 0
    elif echo "$RESULT" | grep -q "specification .* is false"; then
        echo "ERROR: found counterexample outside of specification language:"
        echo "$RESULT" | grep -A 999999 "specification .* is false"
        exit 2
    else
        echo "ERROR: Unknown model checking result"
        echo "$RESULT"
        exit 1
    fi
else
    echo "ERROR: Model checking error"
    echo "$RESULT"
    exit 1
fi
