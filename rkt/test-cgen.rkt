#lang s-exp "racket-base.rkt"
(require
 racket/pretty
 racket/unit

 ;; Signatures
 "field-sig.rkt"
 "field-lib-sig.rkt"
 "main-sig.rkt"

 ;; Generic code
 "field-lib-unit.rkt"
 "test-main-unit.rkt"
 
 ;; Substrate implementation
 "cgen.rkt"
 )

;; Instantiate and introduces identifies into this module's namespace.
(define-values/invoke-unit/infer cgen@)
(define-values/invoke-unit/infer field-lib@)
(define-values/invoke-unit/infer main@)

;; Run the compiler.
(compile-function main)

