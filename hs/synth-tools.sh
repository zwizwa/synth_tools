#!/bin/sh
set -xe
HERE=$(readlink -f $(dirname "$0"))
cd $HERE/..
# There is no point running the Haskell test if the C code doesn't
# compile so do compile first.
echo "make: Entering directory '$(pwd)'"
./make.sh linux/test_fft.dynamic.host.elf

cd $HERE
echo "make: Entering directory '$(pwd)'"
cabal build synth-tools

cd $HERE/..
exec $HERE/synth-tools.elf "$@"
