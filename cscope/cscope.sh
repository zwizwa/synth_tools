#!/bin/sh

# What I want: a cscope file with just the relevant C files.  This is
# sometimes difficult for things like HAL libraries and linux drivers
# because of symbol duplication.

# I've not found a better way to do this than to either list
# everything manually, or do a mix between globbing and manual as done
# here.

cd $(dirname $0)
rm -f cscope.in.out cscope.out cscope.po.out cscope.files

DEPS=$(readlink -f ../../)
# echo "DEPS=$DEPS" >&2

LIBOPENCM3=$DEPS/libopencm3
UC_TOOLS=$DEPS/uc_tools



cat <<EOF >cscope.files
$(ls ../stm32f103/*.[ch])
$(ls ../linux/*.[ch])
$(find $LIBOPENCM3 -name '*.c')
$(find $LIBOPENCM3 -name '*.h')
$(find $UC_TOOLS -name '*.c')
$(find $UC_TOOLS -name '*.h')
EOF

cscope -kbq
