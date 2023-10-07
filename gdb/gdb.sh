#!/bin/sh

# Attempt to make a simpler script.  This always gets out of hand.
# Some design restrictions, considerations.
#
# - This script takes gdb arguments.  I.e. it behaves as "gdb with
#   proper defaults", so will support -i=mi etc...
#
# - The interface is autodetected as much as possible.

if [ -z "$TCP_PORT" ]; then
    TCP_PORT=3333
fi

TMP=/tmp/synth_tools_${TCP_PORT}.gdb
cat <<EOF >$TMP
define connect
    echo port=${TCP_PORT}\n
    target remote localhost:$TCP_PORT
end
startup
EOF

HERE=$(readlink -f $(dirname "$0"))
STM32F103=$HERE/../stm32f103

cd $STM32F103

EXO_DEV=$(exo-dev)
. $EXO_DEV/env
export PATH="$PATH:$EXO_DEV_PATH"

arm-none-eabi-gdb \
    -quiet \
    -x $HERE/lib.gdb \
    -x $TMP \
    "$@"



