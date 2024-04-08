#lang racket/unit
(require (prefix-in base: racket/base))
(require "field-sig.rkt")
;; Evaluator semantics field^ primitives.
(import)
(export field^)
(define (+ s a b) (base:+ a b))
(define (- s a b) (base:- a b))
(define (* s a b) (base:* a b))
(define (/ s a b) (base:/ a b))
