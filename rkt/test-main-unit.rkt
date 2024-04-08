#lang s-exp "dsp.rkt"
;; dsp.rkt redefines: lambda #%app define
;; and exposes basics: require provide
(require
 racket/unit
 "field-sig.rkt"
 "field-lib-sig.rkt"
 "main-sig.rkt"
)

(define-unit main@

  (import field^ field-lib^) 
  (export main^)

  (define (main a b c) (+ a (+ b c)))

)
(provide main@)
