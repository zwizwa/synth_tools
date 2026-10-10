#!/bin/sh
cd $(dirname "$0")
# See https://claude.ai/chat/70d1132d-8cdc-481b-ab52-e273da4c6782
#
# Basic idea:
#
# - Top level make /i/exo/jobserver.mk will invoke us like this:
#   +synth_tools/make.sh
#
# - That '+' indicates that MAKEFLAGS environment variable is set,
#   which contains something like "-j28 --jobserver-auth=fifo:/tmp/GMfifo29453"
#
# - cached-nix-shell will pass that environment variable to the make
#   that runs inside the shell environment
#
# - in case we do not see any MAKEFLAGS, just set them to start a parallel build
#
# - the host.nix file supports both building with nix shell and using
#   an explicitly generated build environment tree (env) that can be
#   deployed on hosts without nix tools
#
# - the old /i/exo/nix config is left here as well

# echo "MAKE=$MAKE"
# echo "MAKEFLAGS=$MAKEFLAGS"

if [ -z "$MAKEFLAGS" ]; then
    export MAKEFLAGS="-j$(nproc)"
    # echo "setting MAKEFLAGS=$MAKEFLAGS"
fi

using_shell() {
    exec cached-nix-shell host.nix -A shell --exec make -C . "$@"
}

using_exo_shell() {
    # This is the old approach which has package builders for all
    # projects, where we can reuse the context to do a local build
    # here.  But it does not expose the build tools separately.
    exec /i/exo/nix/cached-nix-shell.sh synth_tools --exec make "$@"
}

using_env() {
    # If the env is not built then we need to build it 
    if [ ! -d $(readlink -f ./env) ]; then
        echo "Running nix-build to create the missing ./env"
        nix-build host.nix -A env -o env || exit 1
    fi

    # -L uses the link's timestamp instead of the target
    make -L env 
    # export PATH=$(readlink -f env)/bin:$PATH
    export PATH=$(readlink -f env)/bin
    exec make "$@"
}

# using_env "$@"
# using_shell "$@"
using_exo_shell "$@"
