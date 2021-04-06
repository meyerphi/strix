#!/usr/bin/env bash

# Script to intercept the linker call by GraalVM native-image,
# which would create a shared library '.so' or '.dylib' file,
# and additionally create a static library '.a' file.

make_static_ar=false
make_static_libtool=false

SCRIPT=""
LIBTOOL=()
for key in $*; do
    if [[ $key == -* ]]; then
        # linker option
        continue
    elif [[ $key == *.so ]]; then
        # shared library output file (linux)
        make_static_ar=true
        SCRIPT="${SCRIPT}CREATE ${key%.so}.a"$'\n'
    elif [[ $key == *.dylib ]]; then
        # shared library output file (macos)
        make_static_libtool=true
        LIBTOOL+=("-o" "${key%.dylib}.a")
    elif [[ $key == *.o ]]; then
        # object file
        SCRIPT="${SCRIPT}ADDMOD $key"$'\n'
        LIBTOOL+=("$key")
    elif [[ $key == *.a ]]; then
        # archive file
        SCRIPT="${SCRIPT}ADDLIB $key"$'\n'
        LIBTOOL+=("$key")
    fi
done
SCRIPT="${SCRIPT}SAVE"$'\n'
SCRIPT="${SCRIPT}END"

if [ "$make_static_ar" == true ]; then
    # Create static library with GNU ar
    ar -M <<<"$SCRIPT"
    result=$?
    if [ $result -ne 0 ]; then
        exit $result
    fi
fi

if [ "$make_static_libtool" == true ]; then
    # Create static library with OSX libtool
    libtool -static "${LIBTOOL[@]}"
    result=$?
    if [ $result -ne 0 ]; then
        exit $result
    fi
fi

# Call C compiler for compilation or linking step.
# This will also create the shared library in addition
# to the static library.
cc $*
