# FIXME: This still uses RDM_BRIDGE build
[ -z "$RDM_BRIDGE_DEV" ] && RDM_BRIDGE_DEV=~/.nix-c8/rdm-bridge-dev
[ ! -e "$RDM_BRIDGE_DEV" ] && echo "Tools not found in default locations. Set RDM_BRIDGE_DEV to specify." && exit 1
ENV=$RDM_BRIDGE_DEV/env
[ ! -e $ENV ] && echo "RDM_BRIDGE_DEV=$RDM_BRIDGE_DEV does not contain env file" && exit 1
. $ENV
export PATH=$RDM_BRIDGE_DEV_PATH
export LDFLAGS_EXTRA="$RDM_BRIDGE_DEV_LDFLAGS"
export CFLAGS_EXTRA="$RDM_BRIDGE_DEV_CFLAGS"
