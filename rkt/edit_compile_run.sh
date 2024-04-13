#!/bin/sh

inotifywait \
    edit_compile_run.sh \
    sig.rkt \
    field-eval-unit.rkt \
    field-lib-unit.rkt \
    test-main-unit.rkt \
    test-eval.rkt \
    test-cgen.rkt \
    cgen.rkt \
    dsp.rkt \

# racket test-eval.rkt
racket test-cgen.rkt

exec $0

