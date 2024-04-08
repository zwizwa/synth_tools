#lang racket/base
(require
 racket/pretty)
(provide
 (all-defined-out))

(struct cgen (next-reg bindings) #:mutable #:transparent)
(struct reg (nb) #:transparent)
(struct binding (reg op args) #:transparent)
(struct const (value) #:transparent)
(struct arg (nb) #:transparent)

(define (init-cgen) (cgen 0 '()))

(define (make-reg! s)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg nb)))

    

(define (bind s op . args)
  (let* ((r (make-reg! s))
         (b (binding r op args)))
    (set-cgen-bindings! s
     (cons b (cgen-bindings s)))
    r))

(define (op2 op) (lambda (s a b) (bind s op a b)))

(define pp pretty-print)

(define (compile-function main)
  (let* ((state (init-cgen))
         (nb-args (sub1 (procedure-arity main)))
         (args (for/list ((i (in-range nb-args))) (arg i)))
         (result (apply main state args))
         (bindings (cgen-bindings state)))
    (pp args)
    (for ((binding (reverse bindings))) (pp binding))
    (pp result)))


