#!/bin/sh
set -e
source $stdenv/setup
mkdir -p $out/bin
tmp=$(readlink -f .)
cat <<EOF >$out/bin/pd
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
