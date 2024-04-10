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



(struct cgen (next-reg code state stack indices) #:mutable #:transparent)
(define (init-cgen) (cgen 0 '() '() '() '()))

(struct reg (type size tag nb)       #:transparent)
(struct const (value)                #:transparent)
(struct function (state in code out) #:transparent)

(struct bind (reg op args)           #:transparent)
(struct array (reg)                  #:transparent)
(struct assign (dst src)             #:transparent)
(struct array-assign (dst index src) #:transparent)
(struct loop (iter stop code)        #:transparent)
(struct comment (msg)                #:transparent)


(define (make-array-reg! s size tag)
  (let ((nb (cgen-next-reg s)))
    (set-cgen-next-reg! s (add1 nb))
    (reg "T" size tag nb)))

;; Scalar register.
(define (make-reg! s tag)
  (make-array-reg! s '() tag))


(define (make-state! s)
  (let ((r (make-reg! s 's)))
    (set-cgen-state! s (cons r (cgen-state s)))
    r))

(define (compile! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))

(define (bind! s op . args)
  (let* ((r (make-reg! s 'r)))
    (compile! s (bind r op args))
    r))

(define (loop! s iter stop code)
  (compile! s (loop iter stop code)))

(define (assign! s dst src)
  (compile! s (assign dst src)))

(define (array-assign! s dst index src)
  (compile! s (array-assign dst index src)))

(define (comment! s msg)
  (compile! s (comment msg)))

(define (indices s)
  (reverse (cgen-indices s)))

;; Blocks are always loops.  (FIXME: Later, figure out how to
;; implement "if").  Entering a block introduces a new index.  Each
;; point inside a nested series of loops is always associated with a
;; coordinate which is used to collect a block's output.  The language
;; is fundamentally "grid oriented".

(define (enter-block! s index)
  (set-cgen-indices! s (cons index (cgen-indices s)))
  (set-cgen-stack! s (cons (cgen-code s) (cgen-stack s)))
  (set-cgen-code! s '()))

(define (leave-block! s)
  (let* ((code (cgen-code s))
         (stack (cgen-stack s)))
    (set-cgen-indices! s (cdr (cgen-indices s)))
    (set-cgen-stack! s (cdr stack))
    (set-cgen-code! s (car stack))
    (reverse code)))



(define (op2 op)
  ;; (pp op)
  (lambda (s a b)
    (bind! s op a b)))

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
       (_ (comment! s "function body"))
       (out (call-with-values (lambda () (apply main s in)) list))

       ;; Buffer the outputs to make sure they are all registers, and
       ;; perform the assgment.
       (outreg (for/list ((o out)) (make-reg! s 'o)))
       (_ (comment! s "function outputs"))
       (_ (for ((ro outreg) (o out)) (assign! s ro o))))
       
       
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
  (if (pair? elems)
      (cdr (apply append (for/list ((elem elems)) (list between elem))))
      '()))


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
  ;; Array reference
  (define (fmt-index rs)
    (apply string-append (for/list ((r rs)) (format "[~a]" (fmt-reg r)))))
  
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
         (w "  ~a ~a;\n"
            (reg-type r) (fmt-reg r)))
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
           ((comment msg)
            (w "~a// ~a\n"
               (indent) msg))
           ((bind r op args)
            (w "~a~a ~a = ~a(~a);\n"
               (indent) (reg-type r) (fmt-reg r) op (fmt-args args)))
           ((array r)
            (w "~a~a ~a[~a];\n"
               (indent) (reg-type r) (fmt-reg r) (reg-size r)))
           ((assign dst src)
            (w
             ;; assume dst is the same
             ;; see Footnote (2)
             (if (eq? (reg-size src) '())
                 "~a~a = ~a;\n"
                 "~acopy_array(~a, ~a);\n")
             (indent) (fmt-ref dst) (fmt-ref src)))
            
           ((array-assign dst index src)
            (w "~a~a~a = ~a;\n"
               (indent) (fmt-ref dst) (fmt-index index) (fmt-ref src)))
           ((loop iter stop code)
            (begin
              (w "~aloop(~a, ~a) {\n"
                 (indent) (fmt-reg iter) stop)
              (enter!)
              (w-code code)
              (leave!)
              (w "~a}\n"
                 (indent)))
            ))))
  (w-code (function-code f))
  (w "}\n")
  )  


;; For now there is only one datatype: the array.  There is one
;; iteration: the iteration of a state machine.  State output acts as
;; fold, other outputs are accumulated in arrays.  Here s0 is the
;; initial state vector which can be omitted for zero init.  The f is
;; the iterated procedure, n is the number of iterations.
(define (cgen-iterate s n loop-body) ;; . s0
  (comment! s "loop state init")
  (let* ((nb-state (- (procedure-arity loop-body) 2)) ;; (s i . state)
         ;; Before entering the loop, create initialized loop
         ;; variables.  FIXME: Later separate const and non-const.
         (index (bind! s "zero"))
         (state (for/list ((i nb-state)) (bind! s "zero"))))
         
    ;; Enter a new code block.
    (enter-block! s index)
    (comment! s (map reg-nb (indices s)))
    (comment! s "loop state snapshot")

    ;; Buffer the state, see footnote (1).
    (let ((state-in (for/list ((si state)) (bind! s "copy" si))))
    
      ;; FIXME: Compile loop body.  Push current statements to stack.
      ;; (compile-assign state-reg 0) ;; FIXME init
      (comment! s "loop body")
      (call-with-values (lambda () (apply loop-body s index state-in))
        (lambda retvals
          (let*-values
              ;; Where to put the output?
              
              (((state-val out-val) (split-at retvals nb-state))
               ((out) (for/list ((ov out-val)) (make-array-reg! s n 'r))))
            ;; Assign state registers.  Note that these are always scalar
            (comment! s "loop state update")
            (for ((dst state)
                  (src state-val))
                 (assign! s dst src))

            ;; FIXME: The output code is not working properly yet.
            ;; For now focus on making folds work, combined with state
            ;; arrays.

            ;; Assign output registers/cells.  The block output is
            ;; placed in the current hole, which is an abstraction
            ;; that behaves as a struct.  It needs to be a struct
            ;; because multiple return values need to go in multiple
            ;; registers.
            (comment! s "loop output")

            (for ((o out) (ov out-val))
                 (array-assign! s o (indices s) ov))
            
            ;; Finalize basic block and insert the block into the parent
            ;; context.
            (let ((code (leave-block! s)))
              ;; Compile output array declarations before the loop body.
              (for ((o out)) (compile! s (array o)))
              (loop! s index n code))
            
            (apply values (append state out))
            ))))))


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
               ;; Buffer the state, see footnote (1).
               (state-in (for/list ((si state)) (bind! s "copy" si)))
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
                  (for ((dst state) (src state-out)) (assign! s dst src))
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
  ;(define iterate #f)
    
  

  )



;; Footnotes

;; (1) State input expressions are buffered before injecting them into
;;     lambda expressions to avoid them getting passed around, which
;;     could mean they get compiled after their next-state assigment
;;     statements.  It is assumed that the C compiler can easily get
;;     rid of additional assignments.
;;
;; (2) Try to eliminate copy_array() by reference propagation.  This
;;     would only happen for the output so is not that important atm.


