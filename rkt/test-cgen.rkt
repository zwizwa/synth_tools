#lang s-exp "racket-base.rkt"
(require
 racket/pretty
 racket/unit

 "cgen.rkt"
 ;; Signatures
 "field-sig.rkt"
 "field-lib-sig.rkt"
 "main-sig.rkt"
 ;; Unites implementing signatures
 "field-cgen-unit.rkt"
 "field-lib-unit.rkt"
 "test-main-unit.rkt"
 )

;; Instantiate and introduces identifies into this module's namespace.
(define-values/invoke-unit/infer field-cgen@)
(define-values/invoke-unit/infer field-lib@)
(define-values/invoke-unit/infer main@)

(compile-function main)

