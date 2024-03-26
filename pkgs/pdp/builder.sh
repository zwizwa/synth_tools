#!/bin/sh
set -e
# set -x
source $stdenv/setup
mkdir -p $out/bin
tmp=$(readlink -f .)
# ls -l $src
if [ -f $src ]; then
    tar xf $src
else
    cp -a $src pdp-exo
    chmod -R u+wX pdp-exo
fi
cd pdp-exo
ls -l
. bootstrap
./configure
make
ldd pdp.pd_linux
cp -a pdp.pd_linux $out/bin
cp -a abstractions doc $out/

# For now just make a dedicated wrapper for pdp.
# Later, gather all externs in a wrapper.
cat <<EOF >$out/bin/pd-pdp
$pd/bin/pd \
-lib $out/bin/pdp \
-path $out/abstractions \
-path $out/doc/objects \
"\$@"
EOF
chmod +x $out/bin/pd-pdp
