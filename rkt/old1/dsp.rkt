#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 (for-syntax racket/base)
 )
(provide
 #%top ;; FIXME: raise-syntax-error. No toplevel allowed.
 define
 (rename-out
  (dsp-app          #%app)
  (dsp-lambda       lambda)
  (dsp-module-begin #%module-begin)
  )
 ;; Primitive functions
 +
 )

;; PRIM
;;
;; Primitive identifiers will need to be bound somehow.  I want to
;; split up the core language such that it is substrate-independent.
;; The module system doesn't support parameterization.  That would
;; require units but those are cumbersome to use.  So let's assume
;; that a language expression is a closed term where the top level
;; function provides the primitive functions.  Each module then has
;; its own API to inject primitives.



;; MODULE

;; Since we introduce primitives using an outer lambda expression, the
;; module expression will wrap each module level definition with a
;; lambda binding the identifiers.

(define-syntax dsp-module-begin
  (syntax-rules (primitives require)
    ((_ (primitives . prims)
        ;(require lib ...)
        (define name expr) ...)
      (#%plain-module-begin
       ;(require lib ...)
       (provide name ...)
       (define name
         (lambda
             ;; Module-level definitions are parameterized by
             ;; primitives.
             prims
           (syntax-parameterize
            ;; The list will be necessary to instantiate other
            ;; modules when that gets imple
            ((dsp-primitives #'prims))
            expr)))
       ...))))



;; APP / LAM
;;
;; - Compiler state will need to be threaded, so make that the first
;; argument.
;;
;; - Sytax parameters can be used to add a variable to the context
;; when introduced
;;

(define-syntax-parameter dsp-state      #'none)
(define-syntax-parameter dsp-primitives #'none)

;; Functions are represented as functions so we can just apply the
;; context parameter.

(define-syntax dsp-app
  (lambda (stx)
    (syntax-case stx ()
      ((_ f . args)
       #`(f #,(syntax-parameter-value #'dsp-state) . args)))))

(define-syntax dsp-lambda
  (syntax-rules ()
    ((_ args body)
     (lambda (state . args)
       (syntax-parameterize
        ((dsp-state #'state))
        body)))))



(define + "add")
  
