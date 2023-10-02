#!/bin/sh
HERE=$(readlink -f $(dirname "$0"))

if [ -z "$MIDI" ]; then
MIDI=$(ls /dev/midi* | head -n1)
fi
# echo "MIDI=$MIDI" 2>&1
CMD="$HERE/tether_bl_midi.dynamic.host.elf $MIDI"


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
    *)
        # Pass it on
        $CMD "$@"
        ;;
esac

