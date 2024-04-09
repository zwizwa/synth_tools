#lang racket/base
(require
 "field-sig.rkt"
 "close-sig.rkt"
 racket/unit
 racket/list
 racket/match
 racket/pretty)
(provide
 (all-defined-out))


(define logf printf)
(define (log/pp tag item)
  (display tag)
  (pretty-print item))



(struct cgen (next-reg code state) #:mutable #:transparent)
(define (init-cgen) (cgen 0 '() '()))

(struct reg (tag nb) #:transparent)
(struct bind (reg op args) #:transparent)
(struct assign (dst src) #:transparent)
(struct const (value) #:transparent)
(struct function (state in code out) #:transparent)


(define (make-reg! s tag)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg tag nb)))
(define (make-state! s)
  (let ((r (make-reg! s 's)))
    (set-cgen-state! s (cons r (cgen-state s)))
    r))

(define (compile-code! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))

(define (compile-bind! s op . args)
  (let* ((r (make-reg! s 'r)))
    (compile-code! s (bind r op args))
    r))

(define (compile-assign! s dst src)
  (compile-code! s (assign dst src)))

(define (op2 op)
  ;; (pp op)
  (lambda (s a b)
    (compile-bind! s op a b)))

(define pp pretty-print)

;; The result of compiling a collection of nested stream processing
;; functions is one C function parameterized with a state vector.
(define (compile-function main)
  (let*
      ((s (init-cgen))

       ;; Generate registers to serve as function inputs
       (nb-in (sub1 (procedure-arity main)))
       (in (for/list ((i (in-range nb-in))) (make-reg! s 'i)))

       ;; Apply the function, collecting the output expressions.
       (out (call-with-values (lambda () (apply main s in)) list))

       ;; Buffer the outputs to make sure they are all registers, and
       ;; perform the assgment.
       (outreg (for/list ((o out)) (make-reg! s 'o)))
       (_ (for ((ro outreg) (o out)) (compile-assign! s ro o))))
       
       
    ;; Reverse state and code stacks. The in and out lists are already
    ;; in the correct order.
    (function (reverse (cgen-state s)) in (reverse (cgen-code s)) outreg)))


(define (pp-function f)
  (display "state:\n") (pp (function-state f))
  (display "in:\n")    (pp (function-in f))
  (display "code:\n")  (for ((code (function-code f)))
                            (pp code))
  (display "out:\n")   (pp (function-out f)))

(define (intersperse between elems)
  (cdr (apply append (for/list ((elem elems)) (list between elem)))))


(define (fwrite-c-code output-stream f)
  (define (w . args) (apply fprintf output-stream args))
  ;; Register
  (define (fmt-reg r) (format "~a~a" (reg-tag r) (reg-nb r)))
  ;; Register reference.  For local variables (the 'r tag) there are
  ;; no pointer dereferences.  All the rest is in a struct.
  (define (fmt-ref r)
    (if (eq? 'r (reg-tag r))
        (fmt-reg r)
        (format "~a->~a~a" (reg-tag r) (reg-tag r) (reg-nb r))))
  (define (fmt-args args)
    (apply string-append (intersperse ", " (map fmt-ref args))))
  
  ;; Types
  (w "struct state {\n")
    (for ((r (function-state f)))
       (w "  T ~a;\n" (fmt-reg r)))
    (w "};\n")
  (w "struct in {\n")
    (for ((r (function-in f)))
       (w "  T ~a;\n" (fmt-reg r)))
    (w "};\n")
  (w "struct out {\n")
    (for ((r (function-out f)))
       (w "  T ~a;\n" (fmt-reg r)))
    (w "};\n")
  ;; Function
  (w "void update(struct state *s, const struct in *i, struct out *o) {\n")
  (for ((stmt (function-code f)))
       ;; (w "  // ~a\n" stmt)
       (match stmt
         ((bind r op args)
          (w "  T ~a = ~a(~a);\n" (fmt-reg r) op (fmt-args args))
          #f)
         ((assign dst src)
          (w "  ~a = ~a;\n" (fmt-ref dst) (fmt-ref src))
          #f)))
  (w "}\n")
  )  


(define (close-state _ nb-state update)
  (let*
      ;; sub1/add1 account for the extra state parameter that is
      ;; added to dsp functions
      ((nb-in (- (sub1 (procedure-arity update)) nb-state))
       (closed-update
        ;; The processor instance only takes inputs.
        (lambda (s . in)
          (log/pp "instance "  update)
          (let*
              ;; The core principle of the dsp stream language is that
              ;; a stateful stream processor instance corresponds to
              ;; the _application_ of the function that represents it,
              ;; not the function abstraction itself.  This means that
              ;; new state variables corresponding to this instance
              ;; need to be added to the top level C function's state
              ;; when processor representing function is _applied_.
              ((state (for/list ((i (in-range nb-state))) (make-state! s)))
               ;; Buffer the state input expressions in registers so
               ;; the code can't pass state references around which
               ;; can lead to incorrect code if a state reference is
               ;; used after it is updated.
               (state-in (for/list ((si state)) (compile-bind! s "copy" si)))
               )
            
            (log/pp "  state:     " state)
            (log/pp "  state-in:  " state-in)
            (log/pp "  in:        " in)
            (call-with-values
                (lambda () (apply update s (append state-in in)))
              (lambda retvals
                (let*-values
                    (((state-out out) (split-at retvals nb-state)))
                     
                  (log/pp "  state-out: " state-out)
                  (log/pp "  out:       " out)
                  ;; main reason is that these can contain references
                  ;; to state variables
                  (for ((dst state) (src state-out)) (compile-assign! s dst src))
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
