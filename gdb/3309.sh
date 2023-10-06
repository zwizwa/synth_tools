#!/bin/sh
export TCP_PORT=3309
exec $(dirname $0)/gdb.sh "$@"

