#lang racket/unit
(require (prefix-in base: racket/base))
(require
 "cgen.rkt"
 "field-sig.rkt"
 "stream-sig.rkt"
 )
;; Evaluator semantics field^ primitives.
(import)
(export field^ close^)
(define + (op2 "+"))
(define - (op2 "-"))
(define * (op2 "*"))
(define / (op2 "/"))
(define close void)
