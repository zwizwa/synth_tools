#!/bin/sh
export TCP_PORT=3301
exec $(dirname $0)/gdb.sh "$@"

