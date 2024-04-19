#lang typed/racket/base
(require
 "sig.rkt"
 racket/match
 racket/list
 racket/pretty
 )
(provide (all-defined-out))

;; TODO:
;; - Go over all Any, Symbol types

;; (struct const (value)                 #:transparent)
;; (struct function (state in code out)  #:transparent)

(define-type RegTag (U 'i 'o 's 'l 'n 't))
(define-type Opcode String)

(struct reg ([type : Symbol]
             [dims : (Listof dim)]
             [tag  : RegTag]
             [nb   : Integer])
        #:transparent)

(struct dim ([reg  : (U reg #f)]  ;; FIXME: represent differently
             [size : Integer])
        #:transparent)


;; FIXME: RHS can be reg or literal or array ref.  Bundle those into
;; the value type.
(struct bind
        ([reg : reg]
         [op : Opcode]
         [args : (Listof Ref)])
        #:transparent)

(struct array
        ([reg : reg])
        #:transparent)

(struct assign
        ([dst : reg]
         [src : Ref])
        #:transparent)

(struct array-assign
        ([reg : reg]
         [coords : (Listof reg)]
         [src : Ref])
        #:transparent)

;; FIXME: This is the same as slice
(struct array-ref
        ([reg : reg]
         [coords : (Listof Ref)])
        #:transparent)

(struct loop
        ([iter : reg]
         [stop : Any]
         [code : (Listof Code)])
        #:transparent)

(struct comment
        ([msg : String])
        #:transparent)

(define-type Ref (U reg array-ref Number))

(define-type Code (U bind array assign array-assign loop comment))

(struct slice ([parent : reg]
               [coords : (Listof reg)])
        #:transparent)

(struct cgen
        ([next-reg : (Mutable-HashTable RegTag Integer)]
         [code     : (Listof Code)]
         [state    : (Listof reg)]
         [stack    : (Listof (Listof Code))]
         [dims     : (Listof dim)]
         [slice    : (Mutable-HashTable reg slice)]
         [meta : (Listof (Pair reg Any))]
         ) #:mutable #:transparent)

(struct function
        ([state : (Listof reg)]
         [in    : (Listof reg)]
         [code  : (Listof Code)]
         [out   : (Listof reg)]
         )
        #:transparent)

(define (init-cgen)
  (cgen
   (make-hash
    '((i . 0) (o . 0) (s . 0) (l . 0) (n . 0) (t . 0))) ;; next-reg
   '() ;; code
   '() ;; state
   '() ;; stack
   '() ;; dims
   (make-hash) ;; slice
   '() ;; meta
   ))

;; Use a next-reg for each variable tag (i o s l).  This makes it
;; easier to give predictable names to in/out/state structs, and makes
;; generated code easier to read.
(: make-generic-reg!
   (-> cgen Symbol (Listof dim) RegTag
       reg))
(define (make-generic-reg! s type dims tag)
  (let* ((h (cgen-next-reg s))
         (nb (hash-ref h tag)))
    (hash-set! h tag (add1 nb))
    (reg type dims tag nb)))

(: make-array-reg!
   (-> cgen (Listof dim) RegTag
       reg))
(define (make-array-reg! s dims tag)
  (make-generic-reg! s 'T dims tag))

;; Output arrays are constructed as an extra dimension added to
;; the type of a return value.
(: make-out-array-reg!
   (-> cgen dim RegTag reg
       reg))
(define (make-out-array-reg! s dim tag out-val)
  (let ((dims (reg-dims out-val))) ;; FIXME: This might not be a register
    (make-array-reg! s (cons dim dims) tag)))
    



;; Scalar register.
(: make-reg!
   (-> cgen RegTag
       reg))
(define (make-reg! s tag)
  (make-array-reg! s '() tag))

;; This creates an array if it is referenced in a loop context.
(: make-state!
   (-> cgen (Listof dim)
       reg))
(define (make-state! s dims)
  (let ((r (make-array-reg! s dims 's)))
    (set-cgen-state! s (cons r (cgen-state s)))
    r))

(: code!
   (-> cgen Code Void))
(define (code! s c)
  (set-cgen-code! s (cons c (cgen-code s))))


;; (: _nbind! (-> Integer Any))
;; (define (_nbind! n)
;;   (procedure-reduce-arity
;;    (lambda (s tag op . args)
;;      (let* ((r (make-reg! s tag)))
;;        (code! s (bind r op args))
;;        r))
;;    (+ 3 n)))

(: bind0! (-> cgen RegTag Opcode         reg))
(: bind1! (-> cgen RegTag Opcode Ref     reg))
(: bind2! (-> cgen RegTag Opcode Ref Ref reg))
(define (bind0! s tag op)     (let ((r (make-reg! s tag))) (code! s (bind r op (list))) r))
(define (bind1! s tag op a)   (let ((r (make-reg! s tag))) (code! s (bind r op (list a))) r))
(define (bind2! s tag op a b) (let ((r (make-reg! s tag))) (code! s (bind r op (list a b))) r))

;; Create a zero-initialized index variable.
(: index! (-> cgen RegTag reg))
(define (index! s tag)
  (let ((r (make-generic-reg! s 'I '() tag)))
    (code! s (bind r "zero" '()))
    r))

;; FIXME: Get rid of these simple wrappers.

;; (define (loop! s iter stop code)
;;   (code! s (loop iter stop code)))
;; (define (assign! s dst src)
;;   (code! s (assign dst src)))
;; (define (array-assign! s dst coords src)
;;   (code! s (array-assign dst coords src)))
;; (define (comment! s msg)
;;   (code! s (comment msg)))


(: cgen-ref (-> cgen reg reg * Ref))
(define (cgen-ref _ array . coords)
  (array-ref array coords))



;; Blocks are always loops.  For spatial loops, entering a block
;; introduces a new coordinate dimension.  When state is introduced,
;; it is always indexed by the current coordinate because each
;; iteration through a nested loop should have its own stream state.
;; One outer loop can be a time loop, in which case the coordinate is
;; not updated.  State will be iteratively updated for each iteration
;; through that loop.

(: enter-block!
   (-> cgen dim Boolean
       Void))
(define (enter-block! s d is-time)
  (let ((ds (cgen-dims s)))
    (if is-time
        (when (not (eq? ds '()))
          ;; Time loops cannot occur inside space loops.
          (error 'bad-timeloop-nesting))
        ;; Space dimensions (coords + sizes) get tracked.
        (set-cgen-dims! s (cons d ds))))
  (set-cgen-stack! s (cons (cgen-code s) (cgen-stack s)))
  (set-cgen-code! s '()))

(: leave-block!
   (-> cgen Boolean
       (Listof Code)))
(define (leave-block! s is-time)
  (let* ((code (cgen-code s))
         (stack (cgen-stack s)))
    (when (not is-time)
      (set-cgen-dims! s (cdr (cgen-dims s))))
    (set-cgen-stack! s (cdr stack))
    (set-cgen-code! s (car stack))
    (reverse code)))

;; For space loops, the current loop index / coordinate is saved such
;; that state can be constructed in the correct multiplicity,
;; association one state object to a particular loop coordinate.
(: loop-dims
   (-> cgen
       (Listof dim)))
(define (loop-dims s)
  (reverse (cgen-dims s)))

;; Add a slice reference.  Arrays that are returned as values are
;; implemented as slices into parent loop result arrays.
(: def-slice!
   (-> cgen reg reg (Listof reg)
       Void))
(define (def-slice! s reg parent index)
  ;; (log/pp "def-slice!" (list reg parent index))
  (let ((h (cgen-slice s))
        (v (slice parent index)))
    (hash-set! h reg v)))

;; FIXME: Don't use reg-nb to index because counting starts from 0 for
;; each storage class.
(: maybe-slice (-> cgen reg (U slice #f)))
(define (maybe-slice s reg)
  (hash-ref (cgen-slice s) reg #f))

(: op1 (-> Opcode (-> cgen Ref     reg)))
(: op2 (-> Opcode (-> cgen Ref Ref reg)))

(define (op1 op) (lambda (s a)   (bind1! s 'l op a)))
(define (op2 op) (lambda (s a b) (bind2! s 'l op a b)))

(define pp pretty-print)

;; Formatters shared by C code emission and comment formatting.
(: fmt-reg (-> reg String))
(define (fmt-reg r) (format "~a~a" (reg-tag r) (reg-nb r)))

;; Array index, size
(: fmt-array-index (-> (Listof Ref) String))
(define (fmt-array-index rs)
  (apply string-append
         (map (lambda ([r : Ref]) (format "[~a]" (fmt-ref r))) rs)))

(: fmt-array-size (-> (Listof Number) String))
(define (fmt-array-size sizes)
  (apply string-append
         (map (lambda ([size : Number]) (format "[~a]" size)) sizes)))
  

(: dims-regs (-> (Listof dim) (Listof reg)))
(define (dims-regs dims)
  (map (lambda ([d : dim])
         (let ((r (dim-reg d)))
           (assert r reg?)
           r))
       dims))

;; reg or s->mem
(: fmt-reg-or-structmem (-> reg String))
(define (fmt-reg-or-structmem r)
  (let ((tag  (reg-tag r))
        (nb   (reg-nb r))
        (dims (reg-dims r)))
    (case tag
      ((l n t)
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
           (format "~a->~a~a~a" tag tag nb (fmt-array-index (dims-regs dims)))
           ;; All the rest lives in a struct.
           (format "~a->~a~a" tag tag nb))))))

;; Reference or literal value
(: fmt-ref (-> Ref String))
(define (fmt-ref r)
  (match r
    ((? number?)
     (format "~a" r))
    ((array-ref reg index)
     (format "~a~a" (fmt-reg-or-structmem reg) (fmt-array-index index)))
    ((reg type dims tag nb)
     (fmt-reg-or-structmem r))
    ))
  

;; The result of compiling a collection of nested stream processing
;; functions is one C function parameterized with a state vector.
(: compile/list
   (-> cgen
       ;; We take inputs and already evaluated outputs.  Caller needs
       ;; to apply the hoas function to the input probe registers to
       ;; produce out.
       (Listof reg) ;; in
       (Listof reg) ;; out
       function))
(define (compile/list s in out)
  (code! s (comment "function body"))
  (let*
      (;; Buffer the outputs to make sure they are all registers, and
       ;; perform the assgment.
       (outreg (map (lambda ([o : reg]) (make-array-reg! s (reg-dims o) 'o)) out))
       )

    (code! s (comment "function outputs"))
    (for ((ro outreg) (o out))
         (match o
           ((reg type '() tag nb)
            ;; Scalar output value
            (code! s (assign ro o)))
           ((reg type dims tag nb)
            ;; Similar to loop outputs
            (let*
                ((equivalence
                  (format "~a == ~a"
                          (fmt-ref ro) (fmt-ref o)))
                 (msg (format "treat assignment as equivalence: ~a" equivalence)))
              (def-slice! s o ro '())
              (code! s (comment msg))))))

    
    ;; Reverse state and code stacks. The in and out lists are already
    ;; in the correct order.
    (function (reverse (cgen-state s)) in (reverse (cgen-code s)) outreg)))

(: pp-function (-> function Void))
(define (pp-function f)
  (display "state:\n") (pp (function-state f))
  (display "in:\n")    (pp (function-in f))
  (display "code:\n")  (for ((code (function-code f)))
                            (pp code))
  (display "out:\n")   (pp (function-out f)))

(: intersperse (All (S) (-> S (Listof S) (Listof S))))
(define (intersperse between elems)
  (if (pair? elems)
      (cdr (apply
            append
            (map (lambda ([elem : S]) (list between elem)) elems)))
      '()))

(define tab "    ")

(: fmt-args (-> (Listof Ref) String))
(define (fmt-args args)
  (apply string-append (intersperse ", " (map fmt-ref args))))

;; Recursively expand a slice reference.
(: expand-slice (-> cgen slice (Values reg (Listof reg))))
(define (expand-slice s slc)
  (match slc
    ((slice reg coords)
     (let ((pslice (maybe-slice s reg)))
       (if pslice
           (let-values (((preg pcoords) (expand-slice s pslice)))
             (values preg (append pcoords coords)))
           (values reg coords))))))

(: fwrite-c-code (-> cgen function Symbol Output-Port Void))
(define (fwrite-c-code s f ctag output-stream)
  (: w (-> String Any * Void))
  (define (w fmt . args)
    (apply fprintf output-stream fmt args))
  ;; Register



  (define level 0)
  (define (enter!) (set! level (add1 level)))
  (define (leave!) (set! level (sub1 level)))
  (define (indent)
    (apply string-append (make-list (add1 level) tab)))

  ;(w "#ifndef CGEN_OUT_H\n")
  ;(w "#define CGEN_OUT_H\n")
  (w "#include \"cgen_lib.h\"\n")
  
  ;; Structs
  (: w-struct (-> String (-> function (Listof reg)) Void))
  (define (w-struct name field)
    (w "struct ~a_~a {\n" ctag name)
    (for ((r (field f)))
         (let ((dims (reg-dims r)))
           (if (eq? dims '())
               (w "~a~a ~a;\n"   tab (reg-type r) (fmt-reg r))
               (w "~a~a ~a~a;\n" tab (reg-type r) (fmt-reg r) (fmt-array-size (map dim-size dims))))))
    (w "};\n"))
  (w-struct "state" function-state)
  (w-struct "in"    function-in)
  (w-struct "out"   function-out)
    
  ;; Function
  (w "static inline void ~a_update(struct ~a_state *s, const struct ~a_in *i, struct ~a_out *o) {\n"
     ctag ctag ctag ctag)

  (: w-code (-> (Listof Code) Void))
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
            (w "~a~a = ~a;\n"
               (indent) (fmt-ref dst) (fmt-ref src)))
            
           ((array-assign dst coords src)
            (let (;;(_ (log/pp "array-assign" (list dst coords src)))
                  (slice (maybe-slice s dst))
                  (assignment (format "~a~a = ~a"
                                      (fmt-ref dst)
                                      (fmt-array-index coords)
                                      (fmt-ref src))))
              (if slice
                  ;; Recursively substitute slice names to partial
                  ;; array references and append the current
                  ;; coordinate.
                  (let-values (((parent-dst parent-coords) (expand-slice s slice)))
                    ;; (log/pp "expand-slice-rv: " (list parent-dst parent-coords))
                    (w "~a~a~a = ~a; // expanded from: ~a\n"
                       (indent)
                       (fmt-ref parent-dst)
                       (fmt-array-index (append parent-coords coords))
                       (fmt-ref src)
                       assignment
                       ))
                  (w "~a~a\n" (indent) assignment))))

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

  ;(w "#endif\n")

  )  

;; procedure-arity returns:
;; (U (Listof (U Exact-Nonnegative-Integer arity-at-least))
;;    Exact-Nonnegative-Integer
;;    arity-at-least)
;; This needs to be handled using a run time check.
(: procedure-fixed-arity-minus (-> Procedure Nonnegative-Integer
                                   Nonnegative-Integer))
(define (procedure-fixed-arity-minus f n)
  (let ((a (procedure-arity f)))
    (assert a number?)
    (let ((am (- a n)))
      (assert (>= am 0))
      am)))

(: reg-list (-> reg * (Listof reg)))
(define (reg-list . regs)
  (apply list regs))


;; For now there is only one datatype: the array.  There is one
;; iteration: the iteration of a state machine.  State output acts as
;; fold, other outputs are accumulated in arrays.  Here s0 is the
;; initial state vector which can be omitted for zero init.  The f is
;; the iterated procedure, n is the number of iterations.


(: cgen-loop-state-zero!
   (-> cgen
       Nonnegative-Integer ;; nb-state
       (Listof reg)))
(define (cgen-loop-state-zero! s nb-state)
  (for/list ((_ (in-range nb-state)))
            (bind0! s 'l "zero")))


(: cgen-loop-state-from!
   (-> cgen
       (Listof Ref) ;; Initializer exprssions
       (Listof reg)))
(define (cgen-loop-state-from! s ref)
  (for/list ((r ref))
            (bind1! s 'l "copy" r)))

;; Note that we can't constrain the return value of user-defined
;; functions, so this needs to be Ref.
(define-type TargetLoopFunction
  (-> cgen reg (Listof reg) (Listof Ref)))

(: cgen-loop/list
   (-> cgen
       Boolean
       ;; nb-iter number of loop iterations
       Nonnegative-Integer
       ;; state-iniit-or-nb-state indicates the number of states to
       ;; generate, or a thunk that produces a list of Ref to be used
       ;; as state init.
       (U Nonnegative-Integer
          (-> cgen
              '()
              (Listof Ref))) 
       TargetLoopFunction
       (Listof reg)))
(define (cgen-loop/list s is-time nb-iter state-init-or-nb-state loop-body) ;; . s0
  (let*-values
      (;; Before entering the loop, create initialized loop
       ;; variables.  FIXME: Later separate const and non-const.
       ((_) (code! s (comment "loop index init")))
       ((index) (index! s (if is-time 't 'n)))
       ((state nb-state)
        (if (number? state-init-or-nb-state)
            (let ((nb-state state-init-or-nb-state))
              (code! s (comment "loop state zero init"))
              (values
               (cgen-loop-state-zero! s nb-state)
               nb-state))
            (let* ((state-init state-init-or-nb-state)
                   (_ (code! s (comment "state initializer")))
                   (state-ref : (Listof Ref)
                              (state-init s '()))
                   ;; Always make a copy, even if the input is a register!
                   (state-reg : (Listof reg)
                    (cgen-loop-state-from! s state-ref)))
              (values
               state-reg
               (length state-reg)))
            ))
       )
    ;; Enter a new code block.
    (enter-block! s (dim index nb-iter) is-time)
    (code! s (comment "loop state snapshot"))

    (let*-values
        ;; Buffer the state, see footnote (1).
        ((([state-in : (Listof reg)])
          (for/list ((si state))
                    (bind1! s 'l "copy" si)))
         ((_) (code! s (comment "loop body")))
         
         (([retvals : (Listof Ref)] )   (loop-body s index state-in))

         (([state-val : (Listof Ref)]
           [out-val   : (Listof Ref)])  (split-at retvals nb-state))

         ;; FIXME: If a loop function returns a literal (degenerate
         ;; case) then out-reg contains an intermediate register that
         ;; is not properly assigned.

         (([out-reg : (Listof reg)])
          (for/list ((ov out-val))      (as-reg! s ov)))

         (([out-arr : (Listof reg)])    (for/list ((r out-reg))
                                                  (make-out-array-reg!
                                                   s (dim index nb-iter) 'l r))))
      ;; Assign state registers.  These are always scalar
      (code! s (comment "loop state update"))
      (for ((dst state)
            (src state-val))
           (code! s (assign dst src)))

      ;; Output assignment is solved in two steps.  The code
      ;; constructs local 1-dim arrays and uses the current loop index
      ;; to fill them.  This works as long as the references are not
      ;; returned to the enclosing scope.  If that is the case, the
      ;; code needs to be patched in a second step to move the array
      ;; elsewhere.
        
      (code! s (comment "loop output"))
            
      (for ((o  out-arr)
            (ov out-val))
           ;; FIXME: Also handle literals.
           (match ov
             ((reg type dims tag nb)
              ;; If it is an array, some more work is needed to make
              ;; sure we write into the correct location.  At this
              ;; point we know that:
              ;;
              ;; - The 'o' array we created is actually a slice of a
              ;;   parent array.
              (let*
                  ((equivalence
                    (format "~a~a == ~a"
                            (fmt-ref o)
                            (fmt-array-index (list index))
                            (fmt-ref ov)))
                   (msg (format "treat assignment as equivalence: ~a" equivalence)))
                (def-slice! s ov o (list index))
                (code! s (comment msg))))
             (else
              ;; If out-val is a scalar reference then we can just
              ;; copy it.
              (code! s (array-assign o (list index) ov)))
             
             ))
      
            
      ;; Finalize basic block and insert the block into the parent
      ;; context.
      (let ((code (leave-block! s is-time)))
        ;; Compile output array declarations before the loop body.
        (for ((o out-arr)) (code! s (array o)))
        (code! s (loop index nb-iter code)))
      
      (append state out-arr)
      )))


;; Arity is passed elsewhere.
(define-type TargetListFunction
  (-> cgen (Listof Ref) (Listof reg)))

;; FIXME: I can't seem to be able to reconstruct the function type,
;; only Procedure

(: cgen-close/list (-> Nonnegative-Integer ;; nb-state
                       Nonnegative-Integer ;; nb-in
                       TargetListFunction  ;; open function
                       TargetListFunction  ;; closed function
                       ))

;; Sample non-reg references in a reg.  It's a lot simpler to make
;; most of the interfaces work with regs only.  Low level compiler can
;; easily remove unnecessary reg copy operations.
(: as-reg! (-> cgen Ref reg))
(define (as-reg! s ref)
  (if (reg? ref) ref
      ;; FIXME: dims?  Or assume ref is scalar?
      (bind1! s 'l "copy" ref)))

(define (cgen-close/list nb-state nb-in update)
  (lambda (s inref)
    ;; (log/pp "instance "  update)
    (let*
        ((in : (Listof reg)
             (for/list ((r inref)) (as-reg! s r)))
        ;; The core principle of the dsp stream language is that a
        ;; stateful stream processor instance corresponds to the
        ;; _application_ of the function that represents it, not the
        ;; function abstraction itself.  This means that new state
        ;; variables corresponding to this instance need to be added
        ;; to the top level C function's state when processor
        ;; representing function is _applied_.  And one state slot
        ;; needs to be allocated for each point in a (nested) spatial
        ;; iteration.  Note that state registers contain dims (coords
        ;; + sizes), not just coords.
         (state : (Listof reg)
                (for/list ((i (in-range nb-state)))
                          (make-state! s (loop-dims s))))
         ;; (_ (comment! s state))
         ;; Buffer the state, see footnote (1).
         (_ (code! s (comment "feedback state snapshot")))
         (state-in : (Listof reg)
                   (for/list ((si state))
                             (bind1! s 'l "copy" si)))
         (state-in-and-in : (Listof reg)
                          (append state-in in))                
         )
            
            
      ;;(log/pp "  state:     " state)
      ;;(log/pp "  state-in:  " state-in)
      ;;(log/pp "  in:        " in)
      
      (code! s (comment "feedback body"))
           
      (let*-values
          ((([retvals : (Listof reg)])
            (update s state-in-and-in))
           (([state-out : (Listof reg)]
             [out       : (Listof reg)])
            (split-at retvals nb-state)))
        ;;(log/pp "  state-out: " state-out)
        ;;(log/pp "  out:       " out)
        (code! s (comment "feedback state update"))
        (for ((dst state) (src state-out))
             (code! s (assign dst src)))
        out)
      )))


(: in-array! (-> cgen Nonnegative-Integer * reg))
(define (in-array! s . dims)
  (make-array-reg! s (for/list ((d dims)) (dim #f d)) 'i))

(: in-scalar! (-> cgen reg))
(define (in-scalar! s)
  (make-reg! s 'i))

(: cgen-meta! (-> cgen reg Any Void))
(define (cgen-meta! s param itm)
  (set-cgen-meta! s (cons (cons param itm) (cgen-meta s))))



;; Wanted features
;;
;; - Input and output 1D arrays should optionally be referenced by
;;   pointer instead of being defined in the structs.  This makes
;;   interop with existing APIs a bit easier.
;;

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
;;     just a name.  The second pass that performs the substitution is
;;     combined with the c code generation.


