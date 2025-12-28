#!/bin/sh
HERE=$(readlink -f $(dirname "$0"))
cd $HERE/..
exec $HERE/test-cproc.elf "$@"
