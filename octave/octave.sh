#!/bin/sh
cd $(dirname "$0")
export LOADPATH="."

exec octave --eval synth_tools --persist "$@"

