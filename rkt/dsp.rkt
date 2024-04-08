#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 (for-syntax racket/base)
 )
(provide
 #%top ;; FIXME: raise-syntax-error. No toplevel allowed.
 #%module-begin
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
;; Target language funcions are represented as functions that take an
;; extra state argument that is threaded through applications.

;; The lexical variable binding the extra state variable.
(define-syntax-parameter dsp-state #'#f)

(define-syntax dsp-lambda
  (syntax-rules ()
    ((_ args body)
     (lambda (state . args)
       (syntax-parameterize
        ((dsp-state #'state))
        body)))))

(define-syntax dsp-app
 (lambda (stx)
   (syntax-case stx ()
     ((_ f . args)
      #`(#%app f #,(syntax-parameter-value #'dsp-state) . args)))))

(define-syntax dsp-define
  (syntax-rules ()
    ((_ (name arg ...) body)
     (define name (dsp-lambda (arg ...) body)))))

