#lang racket/base
(require
 "field-sig.rkt"
 "stream-sig.rkt"
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



(struct cgen (next-reg code state stack) #:mutable #:transparent)
(define (init-cgen) (cgen 0 '() '() '()))

(struct reg (type  tag nb) #:transparent)
(struct const (value) #:transparent)
(struct function (state in code out) #:transparent)

(struct bind (reg op args) #:transparent)
(struct assign (dst src) #:transparent)
(struct loop (iter code) #:transparent)


(define (make-reg! s tag)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg "T" tag nb)))
(define (make-state! s)
  (let ((r (make-reg! s 's)))
    (set-cgen-state! s (cons r (cgen-state s)))
    r))

(define (compile! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))

(define (compile-bind! s op . args)
  (let* ((r (make-reg! s 'r)))
    (compile! s (bind r op args))
    r))

(define (compile-loop! s iter code)
  (compile! s (loop iter code)))

(define (compile-assign! s dst src)
  (compile! s (assign dst src)))

(define (push-code! s)
  (set-cgen-stack! s (cons (cgen-code s) (cgen-stack s)))
  (set-cgen-code! s '()))

(define (pop-code! s)
  (let* ((code (cgen-code s))
         (stack (cgen-stack s)))
    (set-cgen-stack! s (cdr stack))
    (set-cgen-code! s (car stack))
    code))



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
  ;; Register reference.
  (define (fmt-ref r)
    (let ((tag (reg-tag r)))
      (if (eq? 'r tag)
          ;; Local variables.
          (fmt-reg r)
          ;; All the rest lives in a struct.
          (format "~a->~a~a" tag tag (reg-nb r)))))
  (define (fmt-args args)
    (apply string-append (intersperse ", " (map fmt-ref args))))


  (define level 0)
  (define (enter!) (set! level (add1 level)))
  (define (leave!) (set! level (sub1 level)))
  (define (indent)
    (apply string-append (make-list (add1 level) "  ")))
  
  
  ;; Structs
  (define (w-struct name field)
    (w "struct ~a {\n" name)
    (for ((r (field f)))
         (w "  ~a ~a;\n" (reg-type r) (fmt-reg r)))
    (w "};\n"))
  (w-struct "state" function-state)
  (w-struct "in"    function-in)
  (w-struct "out"   function-out)
    
  ;; Function
  (w "void update(struct state *s, const struct in *i, struct out *o) {\n")
  (define (w-code code)
    (for ((stmt code))
         ;; (w "  // ~a\n" stmt)
         (match stmt
           ((bind r op args)
            (w "~a~a ~a = ~a(~a);\n" (indent) (reg-type r) (fmt-reg r) op (fmt-args args)))
           ((assign dst src)
            (w "~a~a = ~a;\n" (indent) (fmt-ref dst) (fmt-ref src)))
           ((loop iter code)
            (begin
              (w "~aloop(~a) {\n" (indent) (fmt-reg iter))
              (enter!)
              (w "~a// loop body\n" (indent))
              (w-code code)
              (leave!)
              (w "~a}\n" (indent)))
            ))))
  (w-code (function-code f))
  (w "}\n")
  )  

;; For now there is only one datatype: the array.  There is one
;; iteration: the iteration of a state machine.  State output acts as
;; fold, other outputs are accumulated in arrays.  Here s0 is the
;; initial state vector which can be omitted for zero init.  The f is
;; the iterated procedure, n is the number of iterations.
(define (cgen-iterate s n f) ;; . s0
  (let* ((nb-state (- (procedure-arity f) 2)) ;; (s i . state)
         (index (make-reg! s 'r)) ;; FIXME int type
         (out   (make-reg! s 'r)) ;; FIXME int type
         (state (for/list ((i nb-state)) (make-reg! s 'r))))
    (log/pp "nb-state: " nb-state)

    (push-code! s)
    ;; FIXME: Compile loop body.  Push current statements to stack.
    ;; (compile-assign state-reg 0) ;; FIXME init
    (call-with-values (lambda () (apply f s index state))
      (lambda retvals
        (let*-values
            (((state-val out-val) (split-at retvals nb-state)))
          ;; Assign state and output registers.
          ;; FIXME: Think about thow to support more than one out.
          ;; FIXME: These need to use index.
          (for ((dst state)
                (src state-val))
             (compile-assign! s dst src))
          (compile-assign! s out (car out-val))

          ;; Gather code and insert loop body.
          (let ((code (pop-code! s)))
            (compile-loop! s index code))
          
          out
         )))))


(define (cgen-close _ nb-state update)
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
  (export field^ stream^)
  (define + (op2 "add"))
  (define - (op2 "sub"))
  (define * (op2 "mul"))
  (define / (op2 "div"))

  (define close   cgen-close)
  (define iterate cgen-iterate)
    
  

  )
