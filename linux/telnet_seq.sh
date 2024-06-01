#!/bin/sh
set -x
HERE=$(dirname $0)
exec socat TCP-LISTEN:12345,reuseaddr,fork "EXEC:$HERE/telnet_seq.dynamic.host.elf"

# telnet localhost 12345

