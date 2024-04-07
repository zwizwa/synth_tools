#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 (for-syntax racket/base)
 )
(provide
 #%top ;; FIXME: This should raise an error. No toplevel allowed.
 (rename-out
  (dsp-app          #%app)
  (dsp-define       define)
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

;; TOP

;; Since we introduce primitives using an outer lambda expression, the
;; toplevel is best used as expecting that one lambda expression and
;; binding it.

(define-syntax dsp-module-begin
  (syntax-rules ()
    ((_ expr)
     (#%plain-module-begin
      (provide dsp-module)
      (define dsp-module expr)))))



;; APP / LAM
;;
;; - Compiler state will need to be threaded, so make that the first
;; argument.
;;
;; - Sytax parameters can be used to add a variable to the context
;; when introduced
;;

(define-syntax-parameter dsp-eval #'#f)

;; Functions are represented as functions so we can just apply the
;; context parameter.

(define-syntax dsp-app
  (syntax-rules ()
    ((_ f . args) (f "eval" . args))))

(define-syntax dsp-lambda
  (syntax-rules ()
    ((_ args body)
     (lambda (eval . args)
       (syntax-parameterize
        ((dsp-eval #'eval))
        body)))))


(define-syntax dsp-define
  (syntax-rules ()
    ((_ (name args . body))
     (define args (dsp-lambda args . body)))
    ))

(define + "add")
  
