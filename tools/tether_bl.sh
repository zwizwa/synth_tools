#!/bin/sh
HERE=$(readlink -f $(dirname "$0"))

# CMD="$HERE/tether_bl.dynamic.host.elf /dev/ttyACM0"
CMD="$HERE/tether_bl_midi.dynamic.host.elf /dev/midi3"

load_app() {
    $CMD load 0x08004000 $HERE/../stm32f103/$1.x8ab.f103.fw.bin
}

case "$1" in
    loop)
        $CMD wait
        ;;
    synth)
        load_app synth
        ;;
    console)
        load_app console
        ;;
    start)
        $CMD start
        ;;
    *)
        echo "unknown command: $*" >&2
        exit 1
        ;;
esac

