#!/bin/sh

inotifywait \
    edit_compile_run.sh \
    dsp.rkt \
    test_prog.rkt \
    test.rkt
racket test.rkt

exec $0
