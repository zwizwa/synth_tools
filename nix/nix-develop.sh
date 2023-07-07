#!/bin/sh
cd $(dirname $0)
nix \
    --extra-experimental-features nix-command \
    --extra-experimental-features flakes \
    develop \
    "$@"
