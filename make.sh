#!/bin/sh

# Incremental builds bound to the build tools.
HERE=$(dirname "$0")
PWD=$(pwd)

# If EXO_ENV is nonzero we are inside the environment created by
# /i/exo/env.sh which has all tools in the PATH.  This makes
# incremental builds a little faster as no nix expressions need to be
# evaluated.  The build tools available in this environment are those
# installed by nix.build.sh /i/exo/exo-dev/flake.nix in
# ~/.result/exo-dev
#
# Note that the exo redo build needs a make.input file that contains a
# list of all the source files that when changed will retrigger a build.
#
# This is what exo redo will execute:
# (cd /i/exo ; . env.sh ; ./synth_tools/make.sh -C synth_tools)
if [ ! -z "$EXO_ENV" ]; then
    cd $HERE
    exec make -j${NPROC} "$@"
fi

# Otherwise use "nix develop" to expose the build environment defined
# in flake.nix
cd $HERE

# This repository is a flake that can be used with nix develop.
# Use that by default.
if [ -z "$EXO_ENV" ]; then
    NIX=/run/current-system/sw/bin/nix
    exec $NIX develop --print-build-logs --command make -C ${PWD} "$@"
fi
