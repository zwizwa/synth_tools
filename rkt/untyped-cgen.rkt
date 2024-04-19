#lang racket/base
(require
 racket/unit
 racket/match
 racket/pretty
 "sig.rkt"
 "typed-cgen.rkt")
;; Evaluator semantics field^ primitives.
(provide
 (all-defined-out))

;; (define logf printf)
(define (log/pp tag item)
  (display tag)
  (pretty-print item))


;; For now we keep dsp language "dynamically typed at compile time",
;; mostly because it is not clear how to implement typed Racket for
;; the dsp language, nor if it is really necessary to manage
;; complexity.  This makes it easy to keep using multiple arguments /
;; multiple return values.
;;
;; In contrast, the cgen core really needs types just to be able to
;; manage the granularity of the data types.  It represents functions
;; as (Listof Ref) -> (Listof reg), and we perform conversion between
;; multival and list at this end.


;; Convert between multiarg->multival and list->list functions.
(define (m2l f)
  (lambda (s args)
    (call-with-values
        (lambda () (apply f s args))
      list)))
(define (l2m f)
  (lambda (s . args)
    (apply values (f s args))))

(define (compile s main . in)
  ;; Generate code by applying the hoas to the input probes.
  (let ((out ((m2l main) s in)))
    ;; ... and collect function form.
    (compile/list s in out)))


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

;; like m2l above but with extra index argument
(define (m2l-loop f)
  (lambda (s index args)
    (call-with-values
        (lambda () (apply f s index args))
      list)))

(define (loop* s is-time nb-iter maybe-state-init loop-body)
  (let* ((state-init-or-nb-state
          (if maybe-state-init
              (m2l maybe-state-init)
              (- (procedure-arity loop-body) 2)))
         (out
          (cgen-loop/list
           s
           is-time
           nb-iter
           state-init-or-nb-state
           (m2l-loop loop-body))))
             
    (apply values out)))


(define cgen-loop
  (match-lambda*
   ((list s nb-iter loop-body)
    (loop* s #f nb-iter #f loop-body))
   ((list s nb-iter state-init loop-body)
    (loop* s #f nb-iter state-init loop-body))
   ))

(define cgen-timeloop
  (match-lambda*
   ((list s nb-iter loop-body)
    (loop* s #t nb-iter #f loop-body))
   ((list s nb-iter state-init loop-body)
    (loop* s #t nb-iter state-init loop-body))
   ))




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

