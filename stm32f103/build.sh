#!/bin/sh

[ -z "$TYPE" ] && TYPE="$1"
[ -z "$TYPE" ] && TYPE=all

case "$TYPE" in
    *)
        BN=$(basename $0)
        [ -z "$UC_TOOLS" ] && echo "UC_TOOLS undefined" >&2 && exit 1
        exec $UC_TOOLS/gdb/$BN
        ;;
esac







