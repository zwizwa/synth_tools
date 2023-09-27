#!/bin/sh

# Attempt to make a simpler script.  This always gets out of hand.
# Some design restrictions, considerations.
#
# - This script takes gdb arguments.  I.e. it behaves as "gdb with
#   proper defaults", so will support -i=mi etc...
#
# - The interface is autodetected as much as possible.



# TCP_PORT=3307
TCP_PORT=3306

HERE=$(readlink -f $(dirname "$0"))
STM32F103=$HERE/../stm32f103

cd $STM32F103

FILE=bl_midi.core.f103.elf

# FIXME: Centralize EXO_DEV var in a single file
# . $(dirname $0)/../nix-env.sh
# EXO_DEV=/nix/store/c4fp2c3y81qfkih4cx9nhq0wyvnwr1d1-exo-dev
EXO_DEV=$(exo-dev)
. $EXO_DEV/env
export PATH="$PATH:$EXO_DEV_PATH"


arm-none-eabi-gdb \
    -quiet \
    -x $HERE/lib.gdb \
    -ex "file $FILE" \
    -ex "target remote localhost:$TCP_PORT" \



