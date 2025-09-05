#!/bin/sh
cd $(dirname "$0")
exec nix develop --print-build-logs --command cargo "$@"

