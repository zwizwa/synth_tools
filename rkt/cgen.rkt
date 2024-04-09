#lang racket/base
(require
 "field-sig.rkt"
 "close-sig.rkt"
 racket/unit
 racket/list
 racket/pretty)
(provide
 (all-defined-out))

(struct cgen (next-reg code state) #:mutable #:transparent)
(define (init-cgen) (cgen 0 '() '()))

(struct reg (nb) #:transparent)
(struct binding (reg op args) #:transparent)
(struct assignment (dst src) #:transparent)
(struct const (value) #:transparent)
(struct function (state in code out) #:transparent)


(define (make-reg! s)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg nb)))
(define (make-state! s)
  (let ((reg (make-reg! s)))
    (set-cgen-state! s (cons reg (cgen-state s)))
    reg))

(define (code! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))

(define (bind s op . args)
  (let* ((r (make-reg! s)))
    (code! s (binding r op args))
    r))

(define (assign s dst src)
  (code! s (assignment dst src)))

(define (op2 op)
  ;; (pp op)
  (lambda (s a b)
    (bind s op a b)))

(define pp pretty-print)

(define (pp-function f)
  (display "state:\n") (pp (function-state f))
  (display "in:\n")    (pp (function-in f))
  (display "code:\n")  (for ((code (reverse (function-code f))))
                            (pp code))
  (display "out:\n")   (pp (function-out f)))

;; The result of compiling a collection of nested stream processing
;; functions is one C function parameterized with a state vector.
(define (compile-function main)
  (let* ((s (init-cgen))
         (nb-args (sub1 (procedure-arity main)))
         (args (for/list ((i (in-range nb-args))) (make-reg! s)))
         (result (apply main s args))
         (code (cgen-code s))
         (state (cgen-state s)))
    (function state args code result)))

;; Feed back nb-state in/out values via register.
(define (close-impl s nb-state update)
  (let*
      ((nb-in (- (sub1 (procedure-arity update)) nb-state))
       (states (for/list ((i (in-range nb-state))) (make-state! s)))
       (closed-update
        (lambda (s . ins)
          ;(printf "; ins=~a\n" ins)
          (call-with-values
              (lambda () (apply update s (append states ins)))
            (lambda nstates-outs
              (let-values
                  (((nstates outs)
                    (split-at nstates-outs nb-state)))
                ;(printf "; nstates=~a\n; outs=~a\n" nstates outs)
                (for ((state states) (nstate nstates))
                   (assign s state nstate))
                (apply values outs)))))))
    ;(printf "; nb-in=~a nb-state=~a\n" nb-in nb-state)
    ;; (procedure-reduce-arity closed-update (add1 nb-in))
    closed-update))


;; Evaluator semantics field^ primitives.
(define-unit cgen@
  (import)
  (export field^ close^)
  (define + (op2 "+"))
  (define - (op2 "-"))
  (define * (op2 "*"))
  (define / (op2 "/"))

  (define close close-impl)
           
    
  

  )
