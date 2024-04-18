#lang racket/base
(require
 racket/unit
 "sig.rkt"
 "typed-cgen.rkt")
;; Evaluator semantics field^ primitives.
(provide
 (all-defined-out))

;; For now we keep DSP language "dynamically typed at compile time",
;; mostly because it is not clear how to implement typed Racket for
;; the dsp core, nor if it is really necessary.
;;
;; However, the cgen core really needs types just to be able to manage
;; the complexity.

;; With DSP lang dynamically typed, we can keep working with multiple
;; arugments and multiple return values, but perform a translation
;; here at the untyped end before calling the typed/contracted cgen
;; routines.

;; Convert multiarg->multival to list->list
(define (m2l f)
  (lambda (s args)
    (call-with-values
        (lambda () (apply f s args))
      list)))
;; Convert list->list to multiarg->multival
(define (l2m f)
  (lambda (s . args)
    (apply values (f s args)))) 


(define (cgen-close s nb-state update)
  (let*
      ;; first arg is s, rest is state-in followed by in
      ((nb-state+in (sub1 (procedure-arity update))) 
       (nb-in (- nb-state+in nb-state))
       ;; Convert between multiple values in/out and list in/out.
       (update/list (m2l update))
       ;; Close the state i/o
       (closed/list (cgen-close/list nb-state nb-in update/list))
       ;; Transform the closed list in/out to values in/out.
       (closed (l2m closed/list)))
    (procedure-reduce-arity closed (add1 nb-in))))

(define (compile s main . in)
  (let ((out ((m2l main) s in)))
    (compile/list s in out)))


;; Variant foor loop forms
(define (m2l-loop f)
  (lambda (s index args)
    (call-with-values
        (lambda () (apply f s index args))
      list)))
(define (loop* s is-time nb-iter loop-body)
  (let* ((nb-state (- (procedure-arity loop-body) 2)))
    (apply values
           (cgen-loop/list s is-time nb-iter nb-state (m2l-loop loop-body)))))
  
(define (cgen-loop s nb-iter loop-body)
  (loop* s #f nb-iter loop-body))
(define (cgen-timeloop s nb-iter loop-body)
  (loop* s #t nb-iter loop-body))
  



(define (cgen-sizeof _ array)
  (apply values (map dim-size (reg-dims array))))





;; Note that the C gen doesn't generate infix operations to keep
;; things simple.  The C primitives are defined in cgen_lib.h
(define-unit cgen@
  (import)
  (export field^ float^ loop^ stream^ meta^)
  (define + (op2 "add"))
  (define - (op2 "sub"))
  (define * (op2 "mul"))
  (define / (op2 "div"))
  (define frac (op1 "frac"))

  (define close  cgen-close)
  (define loop   cgen-loop)
  (define time   cgen-timeloop)
  (define ref    cgen-ref)
  (define sizeof cgen-sizeof)

  (define meta!  cgen-meta!)

  )

