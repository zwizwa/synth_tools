#!/bin/sh
HERE=$(readlink -f $(dirname "$0"))

# CMD="$HERE/tether_bl.dynamic.host.elf /dev/ttyACM0"
CMD="$HERE/tether_bl_midi.dynamic.host.elf /dev/midi3"

load_app() {
    $CMD load 0x08002800 $HERE/../stm32f103/$1.x8.f103.bin
}

case "$1" in
    loop)
        $CMD wait
        ;;
    synth)
        load_app synth
        ;;
    start)
        $CMD start
        ;;
    *)
        # Pass it on
        $CMD "$@"
        ;;
esac

