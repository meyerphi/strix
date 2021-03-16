#!/bin/bash

# installs the dependencies for verification:
# Spot, ltl2smv, smvtoaig, combine-aiger and nuXmv

# exit on error
set -e
# break when pipe fails
set -o pipefail

echo "Set up installation"

# save folder of script for location of the patches
PATCH_FOLDER=$(readlink -f $(dirname $0))/patches

# collect binaries in this folder
BIN_FOLDER=$HOME/bin
mkdir -p $BIN_FOLDER
echo $BIN_FOLDER >> $GITHUB_PATH

# Spot needs to be built and remain in the binary folder
cd $BIN_FOLDER

# install Spot
if [ -d "$BIN_FOLDER/spot-2.9.6" ]; then
    echo "Using Spot from cache"
else
    echo "Installing Spot"
    wget http://www.lrde.epita.fr/dload/spot/spot-2.9.6.tar.gz
    tar -xzf spot-2.9.6.tar.gz
    cd spot-2.9.6
    ./configure --enable-max-accsets=64
    make
    cd ..
fi
echo $BIN_FOLDER/spot-2.9.6/bin >> $GITHUB_PATH

# build remaining dependecies in temporary folder
cd /tmp

# install NuSMV
if [ -f "$BIN_FOLDER/ltl2smv" ]; then
    echo "Using ltl2smv from cache"
else
    echo "Installing ltl2smv"
    wget http://nusmv.fbk.eu/distrib/NuSMV-2.6.0.tar.gz
    tar -xzf NuSMV-2.6.0.tar.gz
    patch -p0 <$PATCH_FOLDER/nusmv_minisat.patch
    patch -p0 <$PATCH_FOLDER/nusmv_cudd.patch
    patch -p0 <$PATCH_FOLDER/nusmv_cmake.patch
    cd NuSMV-2.6.0/NuSMV
    mkdir build
    cd build
    cmake ..
    make
    cp bin/ltl2smv $BIN_FOLDER
    cd ../../..
    rm -r NuSMV-2.6.0.tar.gz NuSMV-2.6.0
fi

# install aiger tools
if [ -f "$BIN_FOLDER/smvtoaig" ]; then
    echo "Using smvtoaig from cache"
else
    git clone https://github.com/arminbiere/aiger
    cd aiger
    ./configure.sh
    make
    cp smvtoaig $BIN_FOLDER
    cd ..
    rm -rf aiger
fi

# install combine-aiger
if [ -f "$BIN_FOLDER/combine-aiger" ]; then
    echo "Using combine-aiger from cache"
else
    echo "Installing combine-aiger"
    git clone https://github.com/meyerphi/combine-aiger.git
    cd combine-aiger
    make
    cp combine-aiger $BIN_FOLDER
    cd ..
    rm -rf combine-aiger
fi

# install nuXmv
if [ -f "$BIN_FOLDER/nuXmv" ]; then
    echo "Using nuXmv from cache"
else
    echo "Installing nuXmv"
    wget https://es-static.fbk.eu/tools/nuxmv/downloads/nuXmv-2.0.0-linux64.tar.gz
    tar -xzf nuXmv-2.0.0-linux64.tar.gz nuXmv-2.0.0-Linux/bin/nuXmv
    cp nuXmv-2.0.0-Linux/bin/nuXmv $BIN_FOLDER
    rm -r nuXmv-2.0.0-Linux nuXmv-2.0.0-linux64.tar.gz
fi
