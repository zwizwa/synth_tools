#!/bin/sh
PD=$(which pd)
[ -z "$PD" ] && echo "pd not found" && exit 1
PD=$(dirname $(dirname $(readlink -f "$PD")))
# find $PD
# This is written for the exo wrapper.
PD_GUI=$PD/pd-plain/bin/pd-gui
[ ! -x $PD_GUI ] && echo "pd-gui not found" && exit
ls -l $PD_GUI
