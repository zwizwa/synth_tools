#!/bin/sh
cd $(dirname $0)

creb() {
    ../gen_fetch_zwizwa.sh creb exo
    # readlink -f /i/tom/darcs/creb
}

cat <<EOF | tee default.nix
let fetch = { fetchurl }: {
creb = $(creb);
};
in import ./generic.nix fetch
EOF



