#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 racket/unit
 (for-syntax racket/base)
 )
(provide
 #%top ;; FIXME: raise-syntax-error. No toplevel allowed.
 #%module-begin
 define-unit
 require
 provide
 (rename-out
  (dsp-app    #%app)
  (dsp-lambda lambda)
  (dsp-define define)
  )
 )

;; APP / LAM
;;
;; - Compiler state will need to be threaded, so make that the first
;;   argument.
;;
;; - Sytax parameters can be used to add a variable to the context
;;   when introduced
;;

(define-syntax-parameter dsp-state #'#f)

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

(define-syntax dsp-define
  (syntax-rules ()
    ((_ (name arg ...) body)
     (define name (dsp-lambda (arg ...) body)))))

