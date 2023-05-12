#!/bin/sh
HERE=$(dirname "$0")
cd $HERE
echo "make.sh: Entering directory '$HERE'" >&2
. ./nix-env.sh
export LUA=$(readlink -f $(which lua))
export GCC_ARM_NONE_EABI_PREFIX=arm-none-eabi-
NPROC=$(nproc)
echo "NPROC=$NPROC" >&2
make -j$PROC "$@"
