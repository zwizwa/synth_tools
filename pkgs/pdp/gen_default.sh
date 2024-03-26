#!/bin/sh
cd $(dirname $0)

pdp() {
    ../gen_fetch_zwizwa.sh pdp exo
    # readlink -f /i/tom/exo/deps/pdp
}

cat <<EOF | tee default.nix
let fetch = { fetchurl }: {
pdp = $(pdp);
};
in import ./generic.nix fetch
EOF



