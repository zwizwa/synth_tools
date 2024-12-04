#!/bin/sh
[ -z "$1" ] && echo "usage: $0 <host>" && exit 1
nix-copy-closure --to "$1" $(readlink ~/.result/synth_tools)
