#!/bin/bash

# verifies an aiger circuit against a LTL specification
# uses ltlfilt, ltl2smv, smvtoaig, combine-aiger and nuXmv

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

BASE=$(basename $IMPLEMENTATION)
INS_FILE=/tmp/$BASE.ins
OUTS_FILE=/tmp/$BASE.outs
MONITOR_INS_FILE=/tmp/$BASE.monitor.ins
MONITOR_OUTS_FILE=/tmp/$BASE.monitor.outs
SMV_FILE=/tmp/$BASE.monitor.smv
MONITOR_FILE=/tmp/$BASE.monitor.aag
COMBINED_FILE=/tmp/$BASE.combined.aag
RESULT_FILE=/tmp/$BASE.result

function clean_exit {
    exit_code=$1

    # clean temporary files
    rm -f $INS_FILE
    rm -f $OUTS_FILE
    rm -f $SMV_FILE
    rm -f $MONITOR_FILE
    rm -f $COMBINED_FILE
    rm -f $RESULT_FILE

    exit $exit_code
}

# build a monitor for the formula
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    combine_aiger_options=""
else
    combine_aiger_options="--moore"
fi

# verify if inputs and outputs match
if [ -z "$INS" ]; then
    >$INS_FILE
else
    echo $INS | sed -e 's/\s*,\s*/\n/g' | sort >$INS_FILE
fi
if [ -z "$OUTS" ]; then
    >$OUTS_FILE
else
    echo $OUTS | sed -e 's/\s*,\s*/\n/g' | sort >$OUTS_FILE
fi
if [ "$REALIZABLE" == 'UNREALIZABLE' ]; then
    tmp=$INS_FILE
    INS_FILE=$OUTS_FILE
    OUTS_FILE=$tmp
fi
if ! diff -q $INS_FILE <(grep '^i[0-9]* ' $IMPLEMENTATION | sed -e 's/^i[0-9]* //' | sort) >/dev/null; then
    echo "ERROR: Inputs don't match"
    clean_exit 1
fi
if ! diff -q $OUTS_FILE <(grep '^o[0-9]* ' $IMPLEMENTATION | sed -e 's/^o[0-9]* //' | sort) >/dev/null; then
    echo "ERROR: Outputs don't match"
    clean_exit 1
fi

# rewrite formula
LTL_NORMAL=$(ltlfilt --unabbreviate=WMR --nnf -p -f "$LTL")

# create smv file
echo "MODULE main" > $SMV_FILE
echo "  VAR" >> $SMV_FILE
while read i; do
    echo "    $i : boolean;" >> $SMV_FILE
done < $INS_FILE
while read o; do
    echo "    $o : boolean;" >> $SMV_FILE
done < $OUTS_FILE
# smvtoaig crashes if there are no vars
if [ -z "$INS" -a -z "$OUTS" ]; then
    echo "    a : boolean;" >> $SMV_FILE
fi
if [ "$REALIZABLE" == 'REALIZABLE' ]; then
    echo "  LTLSPEC ($LTL_NORMAL)" >> $SMV_FILE
else
    echo "  LTLSPEC !($LTL_NORMAL)" >> $SMV_FILE
fi

# create monitor
smvtoaig -L ltl2smv -a $SMV_FILE >$MONITOR_FILE 2>/dev/null

# combine monitor with implementation
combine-aiger $combine_aiger_options $MONITOR_FILE $IMPLEMENTATION >$COMBINED_FILE

# model check solution
set +e
echo "read_aiger_model -i ${COMBINED_FILE}; encode_variables; build_boolean_model; check_ltlspec_ic3; quit" | nuXmv -int >$RESULT_FILE
result=$?
set -e

# alternative method directly giving nuXmv the formula, not needing ltl2smv and smvtoaig
#if [ "$REALIZABLE" == 'REALIZABLE' ]; then
#    LTL_NORMAL=$(ltlfilt --unabbreviate=WMR -p -f "$LTL" )
#else
#    LTL_NORMAL=$(ltlfilt --unabbreviate=WMR --negate -p -f "$LTL")
#fi
#LTL_NORMAL=$(echo $LTL_NORMAL | sed -e 's/\<1\>/TRUE/g' -e 's/\<0\>/FALSE/g')
#echo "read_aiger_model -i ${IMPLEMENTATION}; encode_variables; build_boolean_model; check_ltlspec_ic3 -p '$LTL_NORMAL'; quit" | nuXmv -int >$RESULT_FILE

# check result
if [ $result -eq 0 ]; then
    if grep -q 'specification .* is true' $RESULT_FILE; then
        clean_exit 0
    elif grep -q "specification .* is false" $RESULT_FILE; then
        echo "FAILURE"
        clean_exit 2
    else
        echo "ERROR: Unknown model checking result"
        clean_exit 1
    fi
else
    echo "ERROR: Model checking error"
    clean_exit 1
fi
