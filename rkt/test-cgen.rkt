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
(define s (init-cgen))
;; FIXME: This should be inferred.  For now just hand it in.

;; Loop sizes are derived from input size.
(define f (compile s main (in-array! s 64)))
(pp-function f)
(display "slices:\n")
;(pp (hash-map (cgen-slice s) cons))
(pp (cgen-slice s))

(define port (open-output-file "../generic/cgen_out.h"  #:exists 'replace))

;; (fwrite-c-code (current-output-port)  f)
(fwrite-c-code port f)

;(define h (make-hash))
;(hash-set! h 'abc 123)
;(pp h)
