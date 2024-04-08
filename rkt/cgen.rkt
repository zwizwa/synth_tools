#lang racket/base
(require
 "field-sig.rkt"
 "close-sig.rkt"
 racket/unit
 racket/list
 racket/pretty)
(provide
 (all-defined-out))

(struct cgen (next-reg statements state) #:mutable #:transparent)
(define (init-cgen) (cgen 0 '() '()))

(struct reg (nb) #:transparent)
(struct binding (reg op args) #:transparent)
(struct assignment (dst src) #:transparent)
(struct const (value) #:transparent)
(struct arg (nb) #:transparent)
(struct function (args statements result) #:transparent)


(define (make-reg! s)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg nb)))
(define (make-state! s)
  (let ((reg (make-reg! s)))
    (set-cgen-state! s (cons reg (cgen-state s)))
    s))

(define (statement! s binding)
  (set-cgen-statements! s (cons binding (cgen-statements s))))

(define (bind s op . args)
  (let* ((r (make-reg! s)))
    (statement! s (binding r op args))
    r))

(define (assign s dst src)
  (statement! s (assignment dst src)))

(define (op2 op) (lambda (s a b) (bind s op a b)))

(define pp pretty-print)

(define (pp-function f)
  (display "args:\n")
  (pp (function-args f))
  (display "statements:\n")
  (for ((statement (reverse (function-statements f)))) (pp statement))
  (display "result:\n")
  (pp (function-result f)))

(define (compile-function main)
  (let* ((state (init-cgen))
         (nb-args (sub1 (procedure-arity main)))
         (args (for/list ((i (in-range nb-args))) (arg i)))
         (result (apply main state args))
         (statements (cgen-statements state))
         (f (function args statements result)))
    (pp-function f)))

;; Evaluator semantics field^ primitives.
(define-unit cgen@
  (import)
  (export field^ close^)
  (define + (op2 "+"))
  (define - (op2 "-"))
  (define * (op2 "*"))
  (define / (op2 "/"))


  
  ;; Feed back nb-state in/out values via register.
  (define (close s nb-state update)
    (let*
        ((nb-in (sub1 (- (procedure-arity update) nb-state)))
         (states (for/list ((i (in-range nb-state))) (make-state! s)))
         (closed-update
          (lambda (_ . ins)
            (call-with-values
                (lambda () (apply update s (append states ins)))
              (lambda (nstates-outs)
                (let-values
                    (((nstates outs)
                      (split-at nstates-outs nb-state)))
                  (for ((state states)
                        (nstate nstates))
                       ;; FIXME Set states
                       (void state nstate))
                  (apply values outs)))))))
      (procedure-reduce-arity
       closed-update nb-in)))
           
    
  

  )
