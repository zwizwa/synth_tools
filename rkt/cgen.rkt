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

(define (compile-code! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))

(define (compile-bind! s op . args)
  (let* ((r (make-reg! s)))
    (compile-code! s (binding r op args))
    r))

(define (compile-assign! s dst src)
  (compile-code! s (assignment dst src)))

(define (op2 op)
  ;; (pp op)
  (lambda (s a b)
    (compile-bind! s op a b)))

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

;; Add nb-state registers to the enclosing target function's state
;; input, and wrap the update function in a new function that reads
;; from the state variables, performs the computation, saves the new
;; state variables and passes on the rest of the outputs.
;;

(define logf printf)
(define (log/pp tag item)
  (display tag)
  (pretty-print item))

(define (close-state s nb-state update)
  (let*
      ;; sub1/add1 account for the extra state parameter that is
      ;; added to dsp functions
      ((nb-in (- (sub1 (procedure-arity update)) nb-state))
       (closed-update
        (lambda (s . in)
          (log/pp "instance "  update)
          (let*
              ;; Note that there is a subtlety here: the state
              ;; variables need to be created when the processor is
              ;; _applied_.  I.e. the stream processor instance
              ;; corresponds to the _application_ of the function that
              ;; represents the stream processor, not the function
              ;; abstraction itself.
              ((state (for/list ((i (in-range nb-state))) (make-state! s))))
            (log/pp "  state: " state)
            (log/pp "  in:    " in)
            (call-with-values
                (lambda () (apply update s (append state in)))
              (lambda retvals
                (let-values
                    (((next out) (split-at retvals nb-state)))
                  (log/pp "  next:  " next)
                  (log/pp "  out:   " out)
                  (for ((dst state) (src next)) (compile-assign! s dst src))
                  (apply values out))))))))
    (procedure-reduce-arity closed-update (add1 nb-in))
    ))


;; Evaluator semantics field^ primitives.

;; Note that the C gen doesn't generate infix operations to keep
;; things simple.  The C primitives are defined as C macros or
;; functions.
(define-unit cgen@
  (import)
  (export field^ close^)
  (define + (op2 "add"))
  (define - (op2 "sub"))
  (define * (op2 "mul"))
  (define / (op2 "div"))

  (define close close-state)
           
    
  

  )
