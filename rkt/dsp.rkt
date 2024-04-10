#lang racket/base ;; -*- scheme-mode -*-
(require
 racket/stxparam
 (for-syntax racket/base)
 )
(provide
 #%module-begin
 #%datum
 require
 provide
 let let*
 (rename-out
  (dsp-app    #%app)
  (dsp-lambda lambda)
  (dsp-define define)
  (dsp-values values)
  (dsp-top    #%top)
  )
 )

;; APP / LAM
;;
;; Target language funcions are represented as functions that take an
;; extra state argument that is threaded through applications.

;; The lexical variable binding the extra state variable.  Note that
;; if we are not inside a dsp-lambda, this will not have a reference
;; to the compiler/evaluator.  Operators that do not rely on it will
;; still work, e.g. 'close'.
(define-syntax-parameter dsp-state #'#f)

(define-syntax dsp-lambda
  (syntax-rules ()
    ((_ args body)
     (lambda (state . args)
       (syntax-parameterize
        ((dsp-state #'state))
        body)))))

(define (dsp-as-function f)
  (cond
   ((procedure? f) f)
   (else (error 'dsp-as-function))))

(define-syntax dsp-app
 (lambda (stx)
   (syntax-case stx ()
     ((_ f . args)
      #`(#%app
         f ;; (dsp-as-function f)
         #,(syntax-parameter-value #'dsp-state)
         . args)))))

(define-syntax dsp-define
  (syntax-rules ()
    ((_ (name arg ...) body)
     (define name (dsp-lambda (arg ...) body)))
    ((_ name expr)
     (define name expr))))

;; This will be called with an extra state argument by dsp-app, so we
;; ignore that.
(define (dsp-values s . vs)
  (apply values vs))

(define-syntax dsp-top
  (lambda (stx)
    (raise-syntax-error #f "no toplevel defined" stx)))
