#lang racket/base
(require (file "test_prog.rkt"))
(define (add eval a b)
  (+ a b))
((dsp-module "eval" add)
 "eval"
 1 2)
