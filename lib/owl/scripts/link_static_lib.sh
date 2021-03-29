#!/usr/bin/env bash

# Script to intercept the linker call by GraalVM native-image,
# which would create a shared library '.so' file, and additionally
# create a static library '.a' file.

make_static=false

SCRIPT=""
for key in $*; do
    if [[ $key == -* ]]; then
        # linker option
        continue
    elif [[ $key == *.so ]]; then
        # shared library output file
        make_static=true
        SCRIPT="${SCRIPT}CREATE ${key%.so}.a"$'\n'
    elif [[ $key == *.o ]]; then
        # object file
        SCRIPT="${SCRIPT}ADDMOD $key"$'\n'
    elif [[ $key == *.a ]]; then
        # archive file
        SCRIPT="${SCRIPT}ADDLIB $key"$'\n'
    fi
done
SCRIPT="${SCRIPT}SAVE"$'\n'
SCRIPT="${SCRIPT}END"

if [ "$make_static" == true ]; then
    # Create static library
    ar -M <<<"$SCRIPT"
    result=$?
    if [ $result -ne 0 ]; then
        exit $result
    fi
fi

# Call C compiler for compilation or linking step.
# This will also create the shared library in addition
# to the static library.
cc $*
