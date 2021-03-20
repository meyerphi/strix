#!/bin/bash

# installs the dependencies for verification:
# Spot and nuXmv

# exit on error
set -e
# break when pipe fails
set -o pipefail

echo "Set up installation"

# collect binaries in this folder
BIN_FOLDER=$HOME/bin
mkdir -p $BIN_FOLDER
echo $BIN_FOLDER >> $GITHUB_PATH

# install Spot
if [ -f "$BIN_FOLDER/ltlfilt" -a -f "$BIN_FOLDER/autfilt" -a -f "$BIN_FOLDER/ltl2tgba" ]; then
    echo "Using Spot from cache"
else
    echo "Installing Spot"
    wget http://www.lrde.epita.fr/dload/spot/spot-2.9.6.tar.gz
    echo '3cc6f69f17f0d1566d68be7040099df70203748b66121354d8ab84d8d13dd3a8  spot-2.9.6.tar.gz' | sha256sum --check
    tar -xzf spot-2.9.6.tar.gz
    cd spot-2.9.6
    # only install needed components
    ./configure --disable-python --enable-max-accsets=64 --bindir=$BIN_FOLDER --libdir=$BIN_FOLDER/lib --datarootdir=/tmp --includedir=/tmp
    make
    make install
    cd ..
    rm -r spot-2.9.6 spot-2.9.6.tar.gz
    # only keep ltlfilt, autfilt, ltl2tgba and the libraries they link to
    for bin in autcross dstar2tgba genaut genltl ltl2tgta ltlcross ltldo ltlgrind ltlsynt randaut randltl; do
        rm $BIN_FOLDER/$bin
    done
    rm $BIN_FOLDER/lib/libspotgen.*
    rm $BIN_FOLDER/lib/libspotltsmin.*
    rm $BIN_FOLDER/lib/*.a
    # further reduce size for cache
    strip $BIN_FOLDER/ltlfilt
    strip $BIN_FOLDER/autfilt
    strip $BIN_FOLDER/ltl2tgba
    strip $BIN_FOLDER/lib/*.so
fi

# install nuXmv
if [ -f "$BIN_FOLDER/nuXmv" ]; then
    echo "Using nuXmv from cache"
else
    echo "Installing nuXmv"
    wget https://es-static.fbk.eu/tools/nuxmv/downloads/nuXmv-2.0.0-linux64.tar.gz
    echo '19ff908008d3af2b198fba93b6dd707103e06a70ed3b462d458e329212cfcd5a  nuXmv-2.0.0-linux64.tar.gz' | sha256sum --check
    tar -xzf nuXmv-2.0.0-linux64.tar.gz nuXmv-2.0.0-Linux/bin/nuXmv
    cp nuXmv-2.0.0-Linux/bin/nuXmv $BIN_FOLDER
    rm -r nuXmv-2.0.0-Linux nuXmv-2.0.0-linux64.tar.gz
fi
