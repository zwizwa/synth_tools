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

cat <<EOF >$out/bin/pd
#/bin/sh
export LD_PRELOAD=$LIBMVEC
$pd/bin/pd \
-lib $creb/bin/creb \
-path $creb/abs \
-path $creb/doc \
-lib $pdp/bin/pdp \
-path $pdp/abstractions \
-path $pdp/doc/objects \
"\$@"
EOF
chmod +x $out/bin/pd
