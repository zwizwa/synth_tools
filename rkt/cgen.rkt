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


;; The dims list uses the same order as C.  Leftmost is outer.

(struct cgen (next-reg code state stack dims slice) #:mutable #:transparent)

(define (init-cgen)
  (cgen
   (make-hash) ;; next-reg
   '() ;; code
   '() ;; state
   '() ;; stack
   '() ;; dims
   (make-hash) ;; slice
   ))


(struct reg (type dims tag nb)       #:transparent)
(struct const (value)                #:transparent)
(struct dim (reg size)               #:transparent)
(struct function (state in code out) #:transparent)
(struct slice (parent index)         #:transparent)

(struct bind (reg op args)           #:transparent)
(struct array (reg)                  #:transparent)
(struct assign (dst src)             #:transparent)
(struct array-assign (reg index src) #:transparent)
(struct array-ref (reg index)        #:transparent)
(struct loop (iter stop code)        #:transparent)
(struct comment (msg)                #:transparent)


;; FIXME: Use a next-reg for each variable type.  This makes it easier
;; to give predictable names to in/out/state structs, and makes code
;; easier to read.
(define (make-generic-reg! s type dims tag)
  (let* ((h (cgen-next-reg s))
         (nb (hash-ref h tag 0)))
    (hash-set! h tag (add1 nb))
    (reg type dims tag nb)))

(define (make-array-reg! s dims tag)
  (make-generic-reg! s "T" dims tag))

;; Scalar register.
(define (make-reg! s tag)
  (make-array-reg! s '() tag))

;; FIXME: This needs to create an array if it is referenced in a loop context.
(define (make-state! s dims)
  (let ((r (make-array-reg! s dims 's)))
    (set-cgen-state! s (cons r (cgen-state s)))
    r))

(define (code! s binding)
  (set-cgen-code! s (cons binding (cgen-code s))))


;; Don't use this one directly
(define (_bind! s tag op . args)
  (let* ((r (make-reg! s tag)))
    (code! s (bind r op args))
    r))
(define (_nbind! n) (procedure-reduce-arity _bind! (+ 3 n)))

(define bind0! (_nbind! 0))
(define bind1! (_nbind! 1))
(define bind2! (_nbind! 2))

;; Create a zero-initialized index variable.
(define (index! s)
  (let ((r (make-generic-reg! s "I" '() 'n)))
    (code! s (bind r "zero" '()))
    r))
  

(define (loop! s iter stop code)
  (code! s (loop iter stop code)))

(define (assign! s dst src)
  (code! s (assign dst src)))

(define (array-assign! s dst index src)
  (code! s (array-assign dst index src)))

(define (comment! s msg)
  (code! s (comment msg)))

(define (dims s)
  (reverse (cgen-dims s)))

(define (cgen-ref _ array . index)
  ;; Note that if the array is an input we can use this access to
  ;; infer the size if the indices are loop variables.  It could also
  ;; be left to the user to specify the input type.  Maybe do that
  ;; first.
  (array-ref array index))

;; Blocks are always loops.  (FIXME: Later, figure out how to
;; implement "if").  Entering a block introduces a new index.  Each
;; point inside a nested series of loops is always associated with a
;; coordinate which is used to collect a block's output.  The language
;; is fundamentally "grid oriented".

(define (enter-block! s index size)
  (set-cgen-dims! s (cons (dim index size) (cgen-dims s)))
  (set-cgen-stack! s (cons (cgen-code s) (cgen-stack s)))
  (set-cgen-code! s '()))

(define (leave-block! s)
  (let* ((code (cgen-code s))
         (stack (cgen-stack s)))
    (set-cgen-dims! s (cdr (cgen-dims s)))
    (set-cgen-stack! s (cdr stack))
    (set-cgen-code! s (car stack))
    (reverse code)))

;; Add a slice reference.  Arrays that are returned as values are
;; implemented as slices into parent loop result arrays.
(define (def-slice! s reg parent index)
  (let ((h (cgen-slice s))
        (v (slice parent index)))
    (hash-set! h reg v)))

;; FIXME: Don't use reg-nb to index because counting starts from 0 for
;; each storage class.
(define (maybe-slice s reg)
  (hash-ref (cgen-slice s) reg #f))

(define (op1 op) (lambda (s a)   (bind1! s 'l op a)))
(define (op2 op) (lambda (s a b) (bind2! s 'l op a b)))

(define pp pretty-print)

;; The result of compiling a collection of nested stream processing
;; functions is one C function parameterized with a state vector.
(define (compile s main . in)
  (comment! s "function body")
  (let*
      (;; Generate registers to serve as function inputs
       ;(nb-in (sub1 (procedure-arity main)))
       ;(in (for/list ((i (in-range nb-in))) (make-reg! s 'i)))

       ;; Apply the function, collecting the output expressions.
       (out (call-with-values (lambda () (apply main s in)) list))

       ;; FIXME: outreg should be more like this
       ;; ((outreg (for/list ((ov out-val)) (make-out-array-reg! s (dim index nb-iter) 'l ov))))

       
       ;; Buffer the outputs to make sure they are all registers, and
       ;; perform the assgment.
       (outreg (for/list ((o out)) (make-reg! s 'o))))

    (comment! s "function outputs")
    (for ((ro outreg) (o out))
         (match o
           ((reg type '() tag nb)
            ;; Scalar output value
            (assign! s ro o))
           ((reg type dims tag nb)
            ;; Similar to loop outputs
            (let*
                ((equivalence
                  (format "~a == ~a;\n"
                          (fmt-ref ro) (fmt-ref o))))
              (def-slice! s o ro '())  ;; FIXME: index
              (comment! s (format "treat assignment as equivalence: ~a" equivalence))))))

    
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

(define tab "    ")

;; Formatters shared by C code emission and comment formatting.
(define (fmt-reg r) (format "~a~a" (reg-tag r) (reg-nb r)))
;; Array index, size
(define (fmt-array-index rs)
  (apply string-append (for/list ((r rs)) (format "[~a]" (fmt-reg r)))))
(define (fmt-array-size sizes)
  (apply string-append (for/list ((size sizes)) (format "[~a]" size))))

;; reg or s->mem
(define (fmt-reg-or-structmem r)
  (let ((tag  (reg-tag r))
        (nb   (reg-nb r))
        (dims (reg-dims r)))
    (case tag
      ((l n)
       ;; Local variables: temporary or index
       (fmt-reg r))
      (else
       ;; All the rest lives in a struct.
       ;;
       ;; State registers can be multi-dimensional, and are always
       ;; associated to specific registers indexing the grid.
       (if (and (eq? tag 's)
                (not (eq? '() dims)))
           ;; Indexed
           (format "~a->~a~a~a" tag tag nb (fmt-array-index (map dim-reg dims)))
           ;; All the rest lives in a struct.
           (format "~a->~a~a" tag tag nb))))))

;; Register reference.
(define (fmt-ref r)
  (match r
    ((array-ref reg index)
     (format "~a~a" (fmt-reg-or-structmem reg) (fmt-array-index index)))
    ((reg type dims tag nb)
     (fmt-reg-or-structmem r))
    ))
  
(define (fmt-args args)
  (apply string-append (intersperse ", " (map fmt-ref args))))

(define (expand-slice s slc)
  (match slc
    ((slice parent index)
     (let ((parent2 (maybe-slice s parent)))
       (if parent2
           (let-values (((r i) (expand-slice s parent2)))
             (values r (append i index)))
           (values parent index))))))

(define (fwrite-c-code s f output-stream)
  (define (w . args) (apply fprintf output-stream args))
  ;; Register



  (define level 0)
  (define (enter!) (set! level (add1 level)))
  (define (leave!) (set! level (sub1 level)))
  (define (indent)
    (apply string-append (make-list (add1 level) tab)))

  (w "#ifndef CGEN_OUT_H\n")
  (w "#define CGEN_OUT_H\n")
  (w "#include \"cgen_lib.h\"\n")
  
  ;; Structs
  (define (w-struct name field)
    (w "struct ~a {\n" name)
    (for ((r (field f)))
         (let ((dims (reg-dims r)))
           (if (eq? dims '())
               (w "~a~a ~a;\n"   tab (reg-type r) (fmt-reg r))
               (w "~a~a ~a~a;\n" tab (reg-type r) (fmt-reg r) (fmt-array-size (map dim-size dims))))))
    (w "};\n"))
  (w-struct "cgen_state" function-state)
  (w-struct "cgen_in"    function-in)
  (w-struct "cgen_out"   function-out)
    
  ;; Function
  (w "static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {\n")
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
            (let ((slicedef
                   (format "~a ~a~a"
                           (reg-type r)
                           (fmt-reg r)
                           (fmt-array-size (map dim-size (reg-dims r))))))
              (if (maybe-slice s r)
                  (w "~a// omit slice definition: ~a\n" (indent) slicedef)
                  (w "~a~a;\n"
                     (indent) slicedef))))

           ((assign dst src)
            (w
             ;; assume dst is the same
             ;; see Footnote (2)
             (if (eq? (reg-dims src) '())
                 "~a~a = ~a;\n"
                 "~acopy_array(~a, ~a);\n")
             (indent) (fmt-ref dst) (fmt-ref src)))
            
           ((array-assign dst index src)
            (let ((slice (maybe-slice s dst))
                  (assignment (format "~a~a = ~a"
                                      (fmt-ref dst)
                              (fmt-array-index index)
                              (fmt-ref src))))
              (if slice
                  (let-values (((parent-dst parent-index) (expand-slice s slice)))
                    
                    (w "~a~a~a = ~a; // expanded from: ~a\n"
                       (indent)
                       (fmt-ref parent-dst)
                       (fmt-array-index (append (list parent-index) index))
                       (fmt-ref src)
                       assignment
                       ))

                  (w "~a~a\n" assignment))))
           ((loop iter stop code)
            (begin
              (let ((fr (fmt-reg iter)))
                (w "~afor(; ~a < ~a; ~a++) {\n"
                   (indent) fr stop fr))
              (enter!)
              (w-code code)
              (leave!)
              (w "~a}\n"
                 (indent)))
            ))))
  (w-code (function-code f))
  (w "}\n")

  (w "#endif\n")

  )  


;; For now there is only one datatype: the array.  There is one
;; iteration: the iteration of a state machine.  State output acts as
;; fold, other outputs are accumulated in arrays.  Here s0 is the
;; initial state vector which can be omitted for zero init.  The f is
;; the iterated procedure, n is the number of iterations.
(define (cgen-loop s nb-iter loop-body) ;; . s0
  (let* ((nb-state (- (procedure-arity loop-body) 2)) ;; (s i . state)
         ;; Before entering the loop, create initialized loop
         ;; variables.  FIXME: Later separate const and non-const.
         (_ (comment! s "loop index init"))
         (index (index! s))
         (_ (comment! s "loop state init"))
         (state (for/list ((i nb-state)) (bind0! s 'l "zero"))))
         
    ;; Enter a new code block.
    (enter-block! s index nb-iter)
    (comment! s "loop state snapshot")

    ;; Output arrays are constructed as an extra dimension added to
    ;; the type of a return value.
    (define (make-out-array-reg! s dim tag out-val)
      (let ((dims (reg-dims out-val))) ;; FIXME: This might not be a register
        (make-array-reg! s (cons dim dims) tag)))
    
    ;; Buffer the state, see footnote (1).
    (let ((state-in (for/list ((si state)) (bind1! s 'l "copy" si))))
    
      (comment! s "loop body")
      (call-with-values (lambda () (apply loop-body s index state-in))
        (lambda retvals
          (let*-values
              (((state-val out-val) (split-at retvals nb-state))
               ((out) (for/list ((ov out-val)) (make-out-array-reg! s (dim index nb-iter) 'l ov))))
            ;; Assign state registers.  These are always scalar
            (comment! s "loop state update")
            (for ((dst state)
                  (src state-val))
                 (assign! s dst src))

            ;; Output assignment is solved in two steps.  The code
            ;; constructs local 1-dim arrays and uses the current loop
            ;; index to fill them.  This works as long as the
            ;; references are not returned to the enclosing scope.  If
            ;; that is the case, the code needs to be patched in a
            ;; second step to move the array elsewhere.
            
            (comment! s "loop output")

            (for ((o out) (ov out-val))
                 ;; FIXME: Also handle literals.
                 (match ov
                   ((reg type '() tag nb)
                    ;; If out-val is a scalar register reference then
                    ;; we can just copy it.
                    (array-assign! s o (list index) ov))
                   ((reg type dims tag nb)
                    ;; If it is an array, some more work is needed to
                    ;; make sure we write into the correct location.
                    ;; At this point we know that:
                    ;;
                    ;; - The 'o' array we created is actually a slice
                    ;;   of a parent array.
                    (let*
                      ((equivalence
                        (format "~a~a == ~a"
                                (fmt-ref o)
                                (fmt-array-index (list index))
                                (fmt-ref ov))))
                      (def-slice! s ov o index)
                      (comment! s (format "treat assignment as equivalence: ~a" equivalence))))
                   ))
            
            ;; Finalize basic block and insert the block into the parent
            ;; context.
            (let ((code (leave-block! s)))
              ;; Compile output array declarations before the loop body.
              (for ((o out)) (code! s (array o)))
              (loop! s index nb-iter code))
            
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
              ((state
                (for/list ((i (in-range nb-state)))
                          (make-state! s (dims
                                          s))))
               ;; (_ (comment! s state))
               ;; Buffer the state, see footnote (1).
               (_ (comment! s "feedback state snapshot"))
               (state-in
                (for/list ((si state))
                          (bind1! s 'l "copy" si)))
               )
            
            (log/pp "  state:     " state)
            (log/pp "  state-in:  " state-in)
            (log/pp "  in:        " in)

            (comment! s "feedback body")
           
            (call-with-values
                (lambda () (apply update s (append state-in in)))
              (lambda retvals
                (let*-values
                    (((state-out out) (split-at retvals nb-state)))
                     
                  (log/pp "  state-out: " state-out)
                  (log/pp "  out:       " out)
                  (comment! s "feedback state update")
                  (for ((dst state) (src state-out)) (assign! s dst src))
                  (apply values out))))))))
    (procedure-reduce-arity closed-update (add1 nb-in))
    ))


(define (cgen-sizeof _ array)
  (apply values (map dim-size (reg-dims array))))

(define (in-array! s . dims)
  (make-array-reg! s (for/list ((d dims)) (dim #f d)) 'i))


;; Evaluator semantics field^ primitives.

;; Note that the C gen doesn't generate infix operations to keep
;; things simple.  The C primitives are defined in cgen_lib.h
(define-unit cgen@
  (import)
  (export field^ stream^)
  (define + (op2 "add"))
  (define - (op2 "sub"))
  (define * (op2 "mul"))
  (define / (op2 "div"))
  (define frac (op1 "frac"))

  (define close  cgen-close)
  (define loop   cgen-loop)
  (define ref    cgen-ref)
  (define sizeof cgen-sizeof)
  

  )



;; Footnotes

;; (1) State input expressions are buffered before injecting them into
;;     lambda expressions to avoid them getting passed around, which
;;     could mean they get compiled after their next-state assigment
;;     statements.  It is assumed that the C compiler can easily get
;;     rid of additional assignments.
;;
;; (2) The first pass defines arrays to collect loop outputs.  This
;;     leads to whole array assigments that get optimized away by
;;     keeping track of which variables are slices, omitting
;;     declarations and assigments, and substituting the refence with
;;     the "C slice" at the point of alement assigment.  This works
;;     beacuse the slice is ferentially transparent, i.e. the slice is
;;     just a name.


