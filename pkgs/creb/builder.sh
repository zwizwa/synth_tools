#!/bin/sh
set -e
# set -x
source $stdenv/setup
mkdir -p $out/bin
tmp=$(readlink -f .)
# ls -l $src
if [ -f $src ]; then
    tar xf $src
    mv creb-exo creb
    chmod -R u+wX creb
else
    cp -a $src creb
    chmod -R u+wX creb
fi
cd creb
make
ls -l creb.pd_linux
cp -a creb.pd_linux $out/bin
cp -a abs $out/
cp -a doc $out/

# For now just make a dedicated wrapper for creb.
# Later, gather all externs in a wrapper.
cat <<EOF >$out/bin/pd-creb
$pd/bin/pd \
-lib $out/bin/creb \
-path $out/abs \
-path $out/doc \
"\$@"
EOF
chmod +x $out/bin/pd-creb


