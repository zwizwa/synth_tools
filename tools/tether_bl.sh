#!/bin/sh
HERE=$(readlink -f $(dirname "$0"))
case "$1" in
    load)
         $HERE/tether_bl.dynamic.host.elf \
             /dev/ttyACM0 \
             load \
             0x08004000 \
             $HERE/../stm32f103/synth.x8ab.f103.fw.bin
         ;;
    start)
         $HERE/tether_bl.dynamic.host.elf \
             /dev/ttyACM0 \
             start
         ;;
esac

