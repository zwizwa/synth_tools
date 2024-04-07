#!/bin/sh

# It doesn't seem that abstracting this further makes any sense.
# These things are always ad-hoc.  Just leave some examples in the
# comments.

# BN=test_cproc
BN=synth
TEST_SH=linux/test_$BN.sh
ELF=linux/$BN.dynamic.host.elf
inotifywait linux/$BN.c; 

make $ELF || exit 1

./$ELF  # Run the bianary
fi
exec $0
