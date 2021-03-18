#!/bin/bash

# exit on error
set -e

origin=$PWD
update=$origin/.github/scripts/git-restore-mtime

# first reset everything to old date (necessary for some directories)
find . -exec touch -t 202101010000.00 {} +

# update main repository
$update

# update submodules
for submodule in $(git config --file .gitmodules --get-regexp path | awk '{ print $2 }'); do
    cd $submodule
    $update
    cd $origin
done

