#!/bin/bash
[ -z "$UC_TOOLS" ] && echo "need UC_TOOLS" && exit 1
exec $UC_TOOLS/stm32f103/$(basename $0)
