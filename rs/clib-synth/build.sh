#!/bin/sh
# Called by do_uc-tools.erl {cargo_build,Dir} rule.
cd $(dirname $0)
cargo build --release
