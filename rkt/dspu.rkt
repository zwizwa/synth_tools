#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 racket/unit
 (for-syntax racket/base)
 )
(provide
 #%top ;; FIXME: raise-syntax-error. No toplevel allowed.
 #%module-begin
 define
 define-unit
 require
 provide
 dsp-app
 dsp-lambda
 dsp-begin
 (rename-out
  (dsp-app          #%app)
  (dsp-lambda       lambda)
  )
 ;; Primitive functions
 +
 )

;; APP / LAM
;;
;; - Compiler state will need to be threaded, so make that the first
;; argument.
;;
;; - Sytax parameters can be used to add a variable to the context
;; when introduced
;;

(define-syntax-parameter dsp-state      #'#f)
(define-syntax-parameter dsp-primitives #'#f)

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

(define-syntax dsp-begin
  (syntax-rules ()
    ((_ expr ...)
     (let-syntax
         ((#%app  #'dsp-app)
          (lambda #'dsp-lambda))
       expr ...))))
