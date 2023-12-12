#!/bin/sh

# Produce a list of files used by make.sh

# This is allowed to be an apporixmation.  The idea is to use this in
# the build system one level up to avoid running make.sh in case none
# of these files changed.

# It is assumed that this will bring us into the directory that
# contains all dependencies.  These can be symlinks (a trailing slash
# is used).
cd $(dirname $0)/..

find synth_tools/ -name '*.c'
find synth_tools/ -name '*.h'
find synth_tools/ -name '*.rs'  | grep -v rs/target
find synth_tools/ -name '*.zig' | grep -v zig-cache

find uc_tools/ -name '*.h'
find uc_tools/ -name '*.c' | grep -v '\\.sm\\.c'


# Notes: don't run this file on every incremental build, instead make
# it depend on make_inputs.sh -- the output of this file does not
# change a lot so best to avoid the find operations which is very
# expensive on NFS.


