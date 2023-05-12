#!/bin/sh

HERE=$(dirname $0)

# For Nix build, which builds from snapshot, not git repo.
if [ ! -e .git ]; then
     echo "$(date '+%Y%m%d-%H%M%S')"
     exit 0
fi

v="$(date '+%Y%m%d-%H%M%S')-$(cd $HERE ; git rev-parse --short=6 HEAD)-dev"
if [ -z "$(cd $HERE ; git diff)" ] ; then
    echo "$v"
else
    echo "$v-dirty"
fi

