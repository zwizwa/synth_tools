#lang racket/base
(require (file "test_prog.rkt"))
(define (add eval a b)
  (+ a b))
;; Instantiate the primitives
(define prog (dsp-module "no-state" add)
  
((dsp-module "eval" add)
 "eval"
 1 2)
