#!/bin/sh

inotifywait \
    edit_compile_run.sh \
    dsp.rkt \
    lib.rkt \
    test_prog.rkt \
    test.rkt \

racket test.rkt

exec $0
