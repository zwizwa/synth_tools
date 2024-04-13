#lang s-exp "dsp.rkt"
;; dsp.rkt redefines: lambda #%app define
;; and exposes basics: require provide
(require
 racket/unit
 "sig.rkt"
)

(define-unit field-lib@

  (import field^)
  (export field-lib^)

  (define (add3 a b c)
    (+ a (+ b c)))

)
(provide field-lib@)
