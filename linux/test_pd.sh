#!/bin/sh

[ -f ~/.pdsettings ] && echo "WARNING: ~/.pdsettings found"


# Run a headless Pd
cd $(dirname "$0")

# This needs the build loop closed, e.g. it runs against a compiled
# version of everything.

SYNTH_TOOLS=~/.result/synth_tools
# ENV=$SYNTH_TOOLS/env
# [ ! $ENV ] && echo "$0 needs ENV=$ENV" && exit 1 ]
# . $ENV
PD=$SYNTH_TOOLS/pd/bin/pd
PD_PLAIN=$SYNTH_TOOLS/pd/pd-plain/bin/pd

# cat $PD ; exec $PD -nogui
echo "PD_PLAIN $(readlink -f $PD_PLAIN)"


PD_ARGS="-nogui" # -verbose
PD_LIBS="-lib synth_tools"
exec $PD_PLAIN $PD_ARGS $PD_LIBS test_pd.pd



