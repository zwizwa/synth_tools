#!/bin/sh
set -e
source $stdenv/setup
mkdir -p $out/bin
tmp=$(readlink -f .)

# FIXME: libmvec doesn't resolve properly.
# I don't understand how to fix this in the build, so preload it here.
# This does smell like a generic problem with plugins...
# I wonder if patchelf needs to run on the .pd_linux .so
LIBMVEC=$(ldd $creb/bin/creb.pd_linux  |grep libmvec | awk '{print $3}')

# Pd needs to work in read-only (embeddded) and read-write
# (development) mode.  It seems simplest to use a directory with
# symlinks in /tmp such that the path can be changed on the fly.

cat <<EOF >$out/bin/pd
#/bin/sh

## FIXME: Move the sleep statements elsewhere.

export LD_PRELOAD=$LIBMVEC
export PATH=$out/pd/bin:\$PATH

[ -z "$DISPLAY" ] && export DISPLAY=:0

# Add an indirection in /tmp for the dependencies.  This allows them
# to be changed while Pd is running, e.g. to switch to "developer
# mode".
# FIXME: There is a back-reference to synth_tools here. Smell?

# If /i/tom/pd is needed then it needs to be added manually.  The main
# purpose of this script is to support compiled synth_tools.

# ln -sf /i/tom/pd /tmp/pd
# -path /tmp/pd/pd
# -path /tmp/pd/pd/abstractions
# -path /tmp/pd/pd/abstractions/compat


mkdir -p /tmp/pd
(cd /tmp/pd
 ln -sf $creb                          creb
 ln -sf $pdp                           pdp
 ln -sf /home/tom/.result/synth_tools  synth_tools
)

exec $pd/bin/pd \
-lib  /tmp/pd/creb/bin/creb \
-path /tmp/pd/creb/abs \
-path /tmp/pd/creb/doc \
-lib  /tmp/pd/pdp/bin/pdp \
-path /tmp/pd/pdp/abstractions \
-path /tmp/pd/pdp/doc/objects \
-lib ~/.result/synth_tools/linux/synth_tools \
-path /tmp/pd/synth_tools/abstractions \
-rt -jack -r 44100 -inchannels 16 -outchannels 16 \
"\$@"


EOF
chmod +x $out/bin/pd

(cd $out
 # Leave a version without libraries loaded for testing.
 ln -s $pd pd-plain
)
