#!/bin/sh
export TCP_PORT=3306
exec $(dirname $0)/gdb.sh "$@"

