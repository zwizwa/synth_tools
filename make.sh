#!/bin/sh
HERE=$(dirname "$0")
PWD=$(pwd)
cd $HERE
## FIXME: exo_vm doesn't see nix command in PATH?
# NIX=nix
NIX=/run/current-system/sw/bin/nix
exec $NIX develop --command make -C ${PWD} "$@"

#####################################################################

# Left here for reference.  Old method is no longer used.

# NOTE: This is a hack to make use of nix-managed dependencies on a
# non-nix system.  It is probably better to use "nix develop" instead.
# See exo-dev/make-synth_tools.sh

NPROC=$(nproc)
# NPROC=1
cd $(dirname $0)

# FIXME: I've recently changed this so the command 'exo-dev' prints
# the path on stdout.  This allows the following:
#
# 1. Use exo-dev so manual edits are no longer necessary.  Assume it
#    is in the path.  Add a workaround later for the external case
#    where exo-dev binary is not available.
#
# 2. Record the /nix/store path in git to allow building on non-nix
#    hosts, or just using cache push.

EXO_DEV=$(exo-dev)
if [ ! -e "$EXO_DEV" ]; then
    echo "exo-dev not found, using last recorded path"
    . ./exo-dev.sh
    exit 1
else
echo "exo-dev is at $EXO_DEV"
cat <<EOF >exo-dev.sh
# See make.sh
EXO_DEV=$EXO_DEV
EOF
fi

. $EXO_DEV/env

export PATH=$EXO_DEV_PATH
export LDFLAGS_EXTRA="$EXO_DEV_LDFLAGS"
export CFLAGS_EXTRA="$EXO_DEV_CFLAGS"

exec make -j${NPROC}




# OLD: Use a full description of the build tools and use
# (cached-)nix-shell to build it.

# This is untenable.  Instead, use the 'exo-dev' approach above, which
# standardizes on a single build environment that can build all
# projects integrated in the exo/redo build.

# 1. Do not use flakes (yet), as I already have a toplevel nixpkgs
#    (zwizwa), and nixpkgs package descriptions that can be used with
#    overlays, so everything is already pinned
#
# 2. Provide a package description for this repository, for eventual
#    release.  For now however only the builddeps are used.  This is
#    structured as a nixpkgs package (nix/pkgs/dev) and a wrapper that
#    binds it to nixpkgs and installs some overlays from nix/pkgs/* if
#    needed.
#
# 3. Use cached-nix-shell to make incremental builds faster
#
# 4. FIXME: uc_tools is not yet part of dev.nix


# NIX_SHELL=nix-shell
NIX_SHELL=cached-nix-shell
$NIX_SHELL nix/dev.nix --run "make -j${NPROC}"

