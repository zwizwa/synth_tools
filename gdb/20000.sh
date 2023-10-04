#!/bin/sh
export TCP_PORT=20000
exec $(dirname $0)/gdb.sh "$@"

