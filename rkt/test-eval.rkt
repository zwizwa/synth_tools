#lang s-exp "racket-base.rkt"
(require
 racket/pretty
 racket/unit
 "field-sig.rkt"
 "field-eval-unit.rkt"
 "field-lib-unit.rkt")

;; Let Racket infer the imports and put the export identifiers in the
;; module namespace.

(define-values/invoke-unit/infer field-eval@)
(define-values/invoke-unit/infer field-lib@)

; add3
(define state (box 0))
(pretty-print
 `((result ,(add3 state 1 2 3))
   (end-state ,(unbox state))))
 
