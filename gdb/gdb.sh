#!/bin/sh

# Attempt to make a simpler script.  This always gets out of hand.
# Some design restrictions, considerations.
#
# - This script takes gdb arguments.  I.e. it behaves as "gdb with
#   proper defaults", so will support -i=mi etc...
#
# - The interface is autodetected as much as possible.



TCP_PORT=3307

HERE=$(readlink -f $(dirname "$0"))
STM32F103=$HERE/../stm32f103
BL_OPEN=$STM32F103/bl_open.core.f103.elf

. $(dirname $0)/../nix-env.sh
arm-eabi-gdb \
    -ex "target remote localhost:$TCP_PORT" \
    -ex "file $BL_OPEN" \



