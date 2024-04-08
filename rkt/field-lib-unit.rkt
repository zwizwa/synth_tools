#lang racket/base
(require
 racket/unit
 "field-sig.rkt"
 "field-lib-sig.rkt")

(define-unit field-lib@
  (import field^)
  (export field-lib^)

  ;; Not entirely clear how to compos this, so create a primitive that
  ;; is correct and then infer macro transformation.
  
  (define add3
    (lambda (state a b c)
      (+ state a (+ state b c))))

  )
(provide field-lib@)
