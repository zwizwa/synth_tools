#!/bin/sh
cd $(dirname "$0")
echo 
echo "begin $0 make"
./make.sh
echo "end $0 make"
echo

echo "begin $0 wait"

inotifywait \
    edit_compile_run.sh \
    sig.rkt \
    field-eval-unit.rkt \
    field-lib-unit.rkt \
    test-main-unit.rkt \
    test-eval.rkt \
    test-cgen.rkt \
    test-complex.rkt \
    untyped-cgen.rkt \
    dsp.rkt \
    typed-cgen.rkt \
    experiment-composition.rkt \
    ../linux/test_cgen.c \


echo "end $0 wait"
echo

exec $0

