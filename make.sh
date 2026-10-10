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
# - the /i/exo/nix shell config is left here as well


# A note on "exporting the nix shell" to have an "open" build
# environment on a non-nix host or a sandbox without nix tools, an
# idea that keeps coming back.  This seems like it is just too hairy.
# It is best to stick to the 'env' approach, i.e. build the tool tree
# explicitly.




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
    # -L uses the link's timestamp instead of the target
    make -L env
    E=$(readlink -f env)

    # Append, so the caller's own tools (git, sed, ...) stay reachable.
    export PATH="$E/bin:$PATH"

    # FIXME: Added by claude (exo session nix_dev_tools): needs review.
    #
    # buildEnv only aggregates files; nothing points the toolchain
    # inside the tree.  rules.mk:324 already compiles host objects with
    # -I$ZWIZWA_DEV/include, and build.sh passes $LDFLAGS to the link,
    # so these three are all that is needed.  The -rpath is what lets
    # the built .elf/.so find libjack and libasound at run time.
    # Use LDFLAGS_EXTRA, not LDFLAGS: uc_tools/linux/env.host.sh:44
    # assigns LDFLAGS unconditionally and would drop ours, while
    # LDFLAGS_EXTRA is passed through untouched (linux/build.sh:74).
    export ZWIZWA_DEV="$E"
    export LDFLAGS_EXTRA="-L$E/lib -Wl,-rpath,$E/lib $LDFLAGS_EXTRA"
    export PKG_CONFIG_PATH="$E/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

    exec make "$@"
}

using_env "$@"
# using_shell "$@"
# using_exo_shell "$@"
