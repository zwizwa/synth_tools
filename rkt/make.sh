#!/bin/sh
# racket test-eval.rkt
# racket typed-cgen.rkt
# racket test-complex.rkt


## Generates ../generic/cgen_test_out.h
racket test-cgen.rkt
(cd .. ; ./make.sh linux/test_cgen.dynamic.host.so)


# racket experiment-composition.rkt

racket test-ffi.rkt 


