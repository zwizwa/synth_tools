#!/bin/sh
set -xe
HERE=$(readlink -f $(dirname "$0"))
cd $HERE/..
# There is no point running the Haskell test if the C code doesn't
# compile so do compile first.
echo "make: Entering directory '$(pwd)'"
./make.sh linux/test_fft.dynamic.host.elf

exec $HERE/test-cproc.elf "$@"
