#!/bin/sh

inotifywait \
    edit_compile_run.sh \
    field-eval-unit.rkt \
    field-lib-unit.rkt \
    field-sig.rkt \
    test-field.rkt \
    dsp.rkt \
    lib.rkt \
    test_prog.rkt \
    test.rkt \

racket test-field.rkt

exec $0


# OLD

inotifywait \
    edit_compile_run.sh \
    dsp.rkt \
    lib.rkt \
    test_prog.rkt \
    test.rkt \

racket test.rkt

exec $0
