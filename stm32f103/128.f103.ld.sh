#!/bin/sh
[ -z "$UC_TOOLS" ] && echo "need UC_TOOLS" && exit 1
exec $UC_TOOLS/gdb/$(basename $0)
