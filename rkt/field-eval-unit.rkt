#lang racket/unit
(require (prefix-in base: racket/base))
(require "field-sig.rkt")
;; Evaluator semantics field^ primitives.
(import)
(export field^)
(define (tick s) (set-box! s (add1 (unbox s))))
(define (+ s a b) (tick s) (base:+ a b))
(define (- s a b) (tick s) (base:- a b))
(define (* s a b) (tick s) (base:* a b))
(define (/ s a b) (tick s) (base:/ a b))
