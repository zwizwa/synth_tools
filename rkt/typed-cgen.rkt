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

(define-type VarTag
  (U 'i ;; Input
     'o ;; Output
     's ;; State
     'v ;; Local variable
     'n ;; Local loop counter
     't ;; Local time counter
     'l ;; Local loop state
     ))
(define-type Opcode String)

(struct var ([type : Symbol]
             [dims : (Listof dim)]
             [tag  : VarTag]
             [nb   : Integer])
        #:transparent)

(struct dim ([var  : (U var #f)]  ;; FIXME: represent differently
             [size : Nonnegative-Integer])
        #:transparent)


;; FIXME: RHS can be var or literal or array ref.  Bundle those into
;; the value type.
(struct bind
        ([var : var]
         [op : Opcode]
         [args : (Listof Ref)])
        #:transparent)

(struct array
        ([var : var])
        #:transparent)

(struct assign
        ([var : var]
         [coords : (Listof var)]
         [src : Ref])
        #:transparent)

;; FIXME: This is the same as slice
(struct array-ref
        ([var : var]
         [coords : (Listof Ref)])
        #:transparent)

(struct loop
        ([iter : var]
         [stop : Any]
         [code : (Listof Code)])
        #:transparent)

(struct seq
        ([code : (Listof Code)])
        #:transparent)

(struct comment
        ([msg : String])
        #:transparent)

;; Ref (reference) is not a good name.  Value would be a better name,
;; but that gives two v's again (var/value) to replace reg/ref.  So
;; keep for now.
(define-type Ref (U var array-ref Number))

(define-type Code (U bind array assign loop seq comment))

(struct slice ([parent : var]
               [coords : (Listof var)])
        #:transparent)

(struct cgen
        ([next-var : (Mutable-HashTable VarTag Integer)]
         [code     : (Listof Code)]
         [state    : (Listof var)]
         [stack    : (Listof (Listof Code))]
         [dims     : (Listof dim)]
         [slice    : (Mutable-HashTable var slice)]
         [meta : (Listof (Pair var Any))]
         ) #:mutable #:transparent)

(struct function
        ([state : (Listof var)]
         [in    : (Listof var)]
         [code  : (Listof Code)]
         [out   : (Listof var)]
         )
        #:transparent)

(define (init-cgen)
  (cgen
   ;; next-var
   (make-hash
    '((i . 0) (o . 0) (s . 0) (v . 0) (n . 0) (t . 0) (l . 0))) 
   '() ;; code
   '() ;; state
   '() ;; stack
   '() ;; dims
   (make-hash) ;; slice
   '() ;; meta
   ))

(: non-scalar-var? (-> Ref Boolean))
(define (non-scalar-var? ref)
  (and (var? ref) (> (length (var-dims ref)) 0)))

;; Use a next-var for each variable tag (i o s l).  This makes it
;; easier to give predictable names to in/out/state structs, and makes
;; generated code easier to read.
(: make-generic-var!
   (-> cgen Symbol (Listof dim) VarTag
       var))
(define (make-generic-var! s type dims tag)
  (let* ((h (cgen-next-var s))
         (nb (hash-ref h tag)))
    (hash-set! h tag (add1 nb))
    (var type dims tag nb)))

(: make-array-var!
   (-> cgen (Listof dim) VarTag
       var))
(define (make-array-var! s dims tag)
  (make-generic-var! s 'T dims tag))

;; Output arrays are constructed as an extra dimension added to
;; the type of a return value.
(: make-out-array-var!
   (-> cgen dim VarTag var
       var))
(define (make-out-array-var! s dim tag out-val)
  (let ((dims (var-dims out-val))) ;; FIXME: This might not be a variable
    (make-array-var! s (cons dim dims) tag)))
    



;; Scalar variable.
(: make-var!
   (-> cgen VarTag
       var))
(define (make-var! s tag)
  (make-array-var! s '() tag))

;; This creates an array if it is referenced in a loop context.
(: make-state!
   (-> cgen (Listof dim)
       var))
(define (make-state! s dims)
  (let ((r (make-array-var! s dims 's)))
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
;;      (let* ((r (make-var! s tag)))
;;        (code! s (bind r op args))
;;        r))
;;    (+ 3 n)))

(: bind0! (-> cgen VarTag Opcode         var))
(: bind1! (-> cgen VarTag Opcode Ref     var))
(: bind2! (-> cgen VarTag Opcode Ref Ref var))
(define (bind0! s tag op)     (let ((r (make-var! s tag))) (code! s (bind r op (list))) r))
(define (bind1! s tag op a)   (let ((r (make-var! s tag))) (code! s (bind r op (list a))) r))
(define (bind2! s tag op a b) (let ((r (make-var! s tag))) (code! s (bind r op (list a b))) r))

;; Create a zero-initialized index variable.
(: index! (-> cgen VarTag var))
(define (index! s tag)
  (let ((r (make-generic-var! s 'I '() tag)))
    (code! s (bind r "zero" '()))
    r))

;; FIXME: Get rid of these simple wrappers.

;; (define (loop! s iter stop code)
;;   (code! s (loop iter stop code)))
;; (define (assign! s dst src)
;;   (code! s (assign dst src)))
;; (define (assign-element! s dst coords src)
;;   (code! s (assign-element dst coords src)))
;; (define (comment! s msg)
;;   (code! s (comment msg)))


(: cgen-ref (-> cgen var var * Ref))
(define (cgen-ref _ array . coords)
  (array-ref array coords))



;; For spatial loops, entering a block introduces a new coordinate
;; dimension.  When state is introduced, it is always indexed by the
;; current coordinate because each iteration through a nested loop
;; should have its own stream state.  One outer loop can be a time
;; loop, in which case the coordinate is not updated.  State will be
;; iteratively updated for each iteration through that loop.

(: enter-loop-block!
   (-> cgen dim Boolean
       Void))
(define (enter-loop-block! s d is-time)
  (let ((ds (cgen-dims s)))
    (if is-time
        (when (not (eq? ds '()))
          ;; Time loops cannot occur inside space loops.
          (error 'bad-timeloop-nesting))
        ;; Space dimensions (coords + sizes) get tracked.
        (set-cgen-dims! s (cons d ds))))
  (enter-block! s))

(: enter-block!
   (-> cgen
       Void))
(define (enter-block! s)
  (set-cgen-stack! s (cons (cgen-code s) (cgen-stack s)))
  (set-cgen-code! s '()))



(: leave-loop-block!
   (-> cgen Boolean
       (Listof Code)))
(define (leave-loop-block! s is-time)
  (when (not is-time)
    (set-cgen-dims! s (cdr (cgen-dims s))))
  (leave-block! s))

(: leave-block!
   (-> cgen
       (Listof Code)))
(define (leave-block! s)
  (let* ((code (cgen-code s))
         (stack (cgen-stack s)))
    (set-cgen-stack! s (cdr stack))
    (set-cgen-code! s (car stack))
    (reverse code)))

(: compile-block! (-> cgen
                      (-> cgen '() (Listof Ref))
                      (Values
                       (Listof Code)
                       (Listof Ref))))
;; Compile code in a fresh context.  Return the outputs of the block
;; as (Listof Ref), and the code as (Listof Code).
(define (compile-block! s block-thunk!)
  ;; (code! s (comment "block"))
  (enter-block! s)
  (let* ((ref
          : (Listof Ref)
          (block-thunk! s '()))
         (code
          : (Listof Code)
          (leave-block! s)))
    (values code ref)))

          
         
            



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
   (-> cgen var var (Listof var)
       Void))
(define (def-slice! s var parent index)
  ;; (log/pp "def-slice!" (list var parent index))
  (let ((h (cgen-slice s))
        (v (slice parent index)))
    (hash-set! h var v)))

;; FIXME: Don't use var-nb to index because counting starts from 0 for
;; each storage class.
(: maybe-slice (-> cgen var (U slice #f)))
(define (maybe-slice s var)
  (hash-ref (cgen-slice s) var #f))

(: op1 (-> Opcode (-> cgen Ref     var)))
(: op2 (-> Opcode (-> cgen Ref Ref var)))

(define (op1 op) (lambda (s a)   (bind1! s 'v op a)))
(define (op2 op) (lambda (s a b) (bind2! s 'v op a b)))

(define pp pretty-print)

;; Formatters shared by C code emission and comment formatting.
(: fmt-var (-> var String))
(define (fmt-var r) (format "~a~a" (var-tag r) (var-nb r)))

;; Array index, size
(: fmt-array-index (-> (Listof Ref) String))
(define (fmt-array-index rs)
  (apply string-append
         (map (lambda ([r : Ref]) (format "[~a]" (fmt-ref r))) rs)))

(: fmt-array-size (-> (Listof Number) String))
(define (fmt-array-size sizes)
  (apply string-append
         (map (lambda ([size : Number]) (format "[~a]" size)) sizes)))
  

(: dims-vars (-> (Listof dim) (Listof var)))
(define (dims-vars dims)
  (map (lambda ([d : dim])
         (let ((r (dim-var d)))
           (assert r var?)
           r))
       dims))

;; var or s->mem
(: fmt-var-or-structmem (-> var String))
(define (fmt-var-or-structmem r)
  (let ((tag  (var-tag r))
        (nb   (var-nb r))
        (dims (var-dims r)))
    (case tag
      ((v n t l)
       ;; Local variables: temporary or index
       (fmt-var r))
      (else
       ;; All the rest lives in a struct.
       ;;
       ;; State variables can be multi-dimensional, and are always
       ;; associated to specific variables indexing the grid.
       (if (and (eq? tag 's)
                (not (eq? '() dims)))
           ;; Indexed
           (format "~a->~a~a~a" tag tag nb (fmt-array-index (dims-vars dims)))
           ;; All the rest lives in a struct.
           (format "~a->~a~a" tag tag nb))))))

;; Reference or literal value
(: fmt-ref (-> Ref String))
(define (fmt-ref r)
  (match r
    ((? number?)
     (format "~a" r))
    ((array-ref var index)
     (format "~a~a" (fmt-var-or-structmem var) (fmt-array-index index)))
    ((var type dims tag nb)
     (fmt-var-or-structmem r))
    ))


;; Single-assignment arrays that are later copied into other arrays
;; can be elimiated by substituting the element-wise assignment in a
;; second pass.
(: slice-equiv-code! (-> cgen String var (Listof var) var Code))
(define (slice-equiv-code! s logtag ro array-index o)
  ;; Similar to loop outputs
  (let*
      ((equivalence
        (format "~a~a == ~a"
                (fmt-ref ro)
                (fmt-array-index array-index)
                (fmt-ref o)))
       (msg (format "~a: treat assignment as equivalence: ~a" logtag equivalence)))
    (def-slice! s o ro array-index)
    (comment msg)))

(: slice-equiv! (-> cgen String var (Listof var) var Void))
(define (slice-equiv! s logtag ro array-index o)
  (code! s (slice-equiv-code! s logtag ro array-index o)))





;; The result of compiling a collection of nested stream processing
;; functions is one C function parameterized with a state vector.
(: compile/list
   (-> cgen
       ;; We take inputs and already evaluated outputs.  Caller needs
       ;; to apply the hoas function to the input probe variables to
       ;; produce out.
       (Listof var) ;; in
       (Listof var) ;; out
       function))
(define (compile/list s in out)
  (let*
      (;; Buffer the outputs to make sure they are all variables, and
       ;; perform the assgment.
       (outvar (map (lambda ([o : var]) (make-array-var! s (var-dims o) 'o)) out))
       )

    (code! s (comment "function outputs"))
    (for ((ro outvar) (o out))
         (match o
           ((var type '() tag nb)
            ;; Scalar output value
            (code! s (assign ro '() o)))
           ((var type dims tag nb)
            (slice-equiv! s "top-out" ro '() o))))

    
    ;; Reverse state and code lists (stacks). The in and out lists are
    ;; already in the correct order.
    (function (reverse (cgen-state s))
              in
              (reverse (cgen-code s))
              outvar)))

(: pp-function (-> function Void))
(define (pp-function f)
  (display ";; -*- scheme -*-\n")
  (display ";; state:\n") (pp (function-state f))
  (display ";; in:\n")    (pp (function-in f))
  (display ";; code:\n")  (for ((code (function-code f))) (pp code))
  (display ";; out:\n")   (pp (function-out f)))

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
(: expand-slice (-> cgen slice (Values var (Listof var))))
(define (expand-slice s slc)
  (match slc
    ((slice var coords)
     (let ((pslice (maybe-slice s var)))
       (if pslice
           (let-values (((pvar pcoords) (expand-slice s pslice)))
             (values pvar (append pcoords coords)))
           (values var coords))))))

(: fwrite-c-code (-> cgen function Symbol Output-Port Void))
(define (fwrite-c-code s f ctag output-stream)
  (: w (-> String Any * Void))
  (define (w fmt . args)
    (apply fprintf output-stream fmt args))
  ;; Variable



  (define level 0)
  (define (enter!) (set! level (add1 level)))
  (define (leave!) (set! level (sub1 level)))
  (define (indent)
    (apply string-append (make-list (add1 level) tab)))

  ;(w "#ifndef CGEN_OUT_H\n")
  ;(w "#define CGEN_OUT_H\n")
  (w "#include \"cgen_lib.h\"\n")
  
  ;; Structs
  (: w-struct (-> String (-> function (Listof var)) Void))
  (define (w-struct name field)
    (w "struct ~a_~a {\n" ctag name)
    (for ((r (field f)))
         (let ((dims (var-dims r)))
           (if (eq? dims '())
               (w "~a~a ~a;\n"   tab (var-type r) (fmt-var r))
               (w "~a~a ~a~a;\n" tab (var-type r) (fmt-var r) (fmt-array-size (map dim-size dims))))))
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
               (indent) (var-type r) (fmt-var r) op (fmt-args args)))

           ((array r)
            (let ((slicedef
                   (format "~a ~a~a"
                           (var-type r)
                           (fmt-var r)
                           (fmt-array-size (map dim-size (var-dims r))))))
              (if (maybe-slice s r)
                  (w "~a// omit slice definition: ~a\n"
                     (indent) slicedef)
                  (w "~a~a;\n"
                     (indent) slicedef))))

           ((assign dst coords src)
            (let (;;(_ (log/pp "assign-element" (list dst coords src)))
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
                  (w "~a~a;\n" (indent) assignment))))

           ((loop iter stop code)
            (begin
              (let ((fr (fmt-var iter)))
                (w "~afor(; ~a < ~a; ~a++) {\n"
                   (indent) fr stop fr))
              (enter!)
              (w-code code)
              (leave!)
              (w "~a}\n"
                 (indent))))

          ((seq code)
            (begin
              (w "~a{\n" (indent))
              (enter!)
              (w-code code)
              (leave!)
              (w "~a}\n"
                 (indent))))
           
          )))
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

(: var-list (-> var * (Listof var)))
(define (var-list . vars)
  (apply list vars))


;; For now there is only one datatype: the array.  There is one
;; iteration: the iteration of a state machine.  State output acts as
;; fold, other outputs are accumulated in arrays.  Here s0 is the
;; initial state vector which can be omitted for zero init.  The f is
;; the iterated procedure, n is the number of iterations.

(: cgen-loop-state-from!
   (-> cgen
       (-> cgen '() (Listof Ref))
       (Values
        (Listof Code)
        (Listof var))))
(define (cgen-loop-state-from! s state-init)
  (let*-values
      (;; Instantiate the state initialization code that goes before
       ;; the loop body.  This gives us a list of refs to use to
       ;; initialize the state variables.
       (([code : (Listof Code)]
         [ref : (Listof Ref)])
        (compile-block! s state-init))
       (([statevar : (Listof var)])
        (for/list
         ((r : Ref ref))
         (make-generic-var!
          s 'T
          (if (var? r)
              (var-dims r)
              '()) ;; FIXME: Is non-var always a scalar?
          'l)))
       ;; There are few constraints on the initializer expressions,
       ;; but we really need to make sure that slice equivalences are
       ;; expressed such that the inner element-wise assigment is made
       ;; to space allocated inside a state array.  For each of the
       ;; state initializer expressions, determine if that is the
       ;; case.
       (([initcode : (Listof (Listof Code))])
        (for/list
         ((r : Ref ref)
          (sv : var statevar))
         ;; Can it just always do a slice equivalence?  I mean a
         ;; scalar variable is just a degenerate case of a grid
         ;; element variable.
         (list
          (if (var? r)
              (slice-equiv-code! s "ls-from!" sv '() r)
              ;; This happens e.g. for zero initializers.
              ;; FIXME: Implement this path.  For now just let it pass.
              ;; (comment (format "FIXME: non-var initcode: ~a ~a" sv r))
              ;; FIXME: Is this always ok?
              (assign sv '() r)
              ))))
       )
    (values
     (append code (apply append initcode))
     statevar)))


;; With slice equivalence subsititution (in-place updates of state) we
;; cannot guarantee the order of state input reads and state output
;; writes, so in all cases the state inputs are copied into an
;; immutable variable before being passed to the body of a function.
;; For arrays this needs to insert a loop. FIXME


;; FIXME: There is a bug here still.  If RHS contains a slice
;; alias it needs to be expanded.

(: snapshot! (-> cgen (Listof var)
                 (Listof var)))
(define (snapshot! s state)
  (code! s (comment "loop state snapshot"))
  (for/list
      ((si state))
    (if (= (length (var-dims si)) 0)


        (bind1! s 'v "copy" si)
        ;; FIXME: Make it work for multidim also.
        ;; FIXME: Make it work tout court.
        (let ((n (dim-size (car (var-dims si)))))
          (car
           (cgen-loop/list
            s #f n
            (lambda (s _) (list))
            (lambda (s i args)
              (list (cgen-ref s si i)))))))))
                                      




;; Note that we can't constrain the return value of user-defined
;; functions, so this needs to be Ref.
(define-type TargetLoopFunction
  (-> cgen var (Listof var) (Listof Ref)))


;; FIXME: Convert empty init to function that generates zeros.  While
;; compiling the init body, convert array outputs to reference loops.
;; Make sure all outputs are new variables.  Then they can be reused
;; as state.

(: cgen-loop/list
   (-> cgen
       Boolean
       ;; nb-iter number of loop iterations
       Nonnegative-Integer
       ;; state-init
       (-> cgen '() (Listof Ref))
       TargetLoopFunction
       (Listof var)))
(define (cgen-loop/list s is-time nb-iter state-init loop-body) ;; . s0
  (let*-values
      (;; Before entering the loop, create initialized loop
       ;; variables.  FIXME: Later separate const and non-const.
       (([init-code : (Listof Code)]
         [state     : (Listof var)])
        (cgen-loop-state-from! s state-init))
       ((nb-state) (length state))
       ;;((_) (code! s (comment "loop index init"))) ;; This is self-evident
       ((index) (index! s (if is-time 't 'n)))
       )
    ;; Enter a new code block.
    (enter-loop-block! s (dim index nb-iter) is-time)
    

    (let*-values
        ;; Buffer the state, see footnote (1).
        ((([state-in : (Listof var)]) (snapshot! s state))
         ((_) (code! s (comment "loop body")))
         (([retvals : (Listof Ref)] )   (loop-body s index state-in))

         (([state-val : (Listof Ref)]
           [out-val   : (Listof Ref)])  (split-at retvals nb-state))

         ;; FIXME: If a loop function returns a literal (degenerate
         ;; case) then out-var contains an intermediate variable that
         ;; is not properly assigned.

         ((_) (code! s (comment "loop body output as var")))
         (([out-var : (Listof var)])
          (for/list ((ov out-val))      (as-var! s ov)))

         (([out-arr : (Listof var)])    (for/list ((r out-var))
                                                  (make-out-array-var!
                                                   s (dim index nb-iter) 'v r))))
      ;; Assign state variables.
      (code! s (comment "loop state update"))
      (for ((dst state)
            (src state-val))
           (if (and (var? src) (> (length (var-dims src)) 0))
               (slice-equiv! s "loop-state-update" dst '() src)
               ;; FIXME: Other cases?
               (code! s (assign dst '() src))))

      ;; Output assignment is solved in two steps.  The code
      ;; constructs local 1-dim arrays and uses the current loop index
      ;; to fill them.  This works as long as the references are not
      ;; returned to the enclosing scope.  If that is the case, the
      ;; code needs to be patched in a second step to move the array
      ;; elsewhere.
        
      (code! s (comment "loop output"))
            
      (for ((a out-arr)
            (r out-var))
           (match r
             ;; FIXME: If an assignment is to a state variable it
             ;; should never be made equivalent.
             ((var type '() tag nb)
              ;; If rval is not an array, just assign it.
              (code! s (assign a (list index) r)))
             ((var type dims tag nb)
              ;; If it is an array, some more work is needed to make
              ;; sure we write into the correct location.  At this
              ;; point we know that:
              ;;
              ;; - The 'o' array we created is actually a slice of a
              ;;   parent array.
              (slice-equiv! s "loop-out" a (list index) r))
             (else
              ;; If out-val is a scalar reference then we can just
              ;; copy it.
              (code! s (assign a (list index) r)))
             ))
      
            
      ;; Finalize basic block and insert the block into the parent
      ;; context.
      (let ((code (leave-loop-block! s is-time)))
        ;; Compile output array declarations before the loop body.
        (for ((o out-arr)) (code! s (array o)))
        ;; Compile state variable/array declarations before the loop body.
        (for ((sv state)) (code! s (array sv)))
        ;; Copile initializer code
        (for ((c init-code)) (code! s c))
        
        ;; Loop body
        (code! s (loop index nb-iter code)))
      
      (append state out-arr)
      )))


;; Arity is passed elsewhere.
(define-type TargetListFunction
  (-> cgen (Listof Ref) (Listof var)))

;; FIXME: I can't seem to be able to reconstruct the function type,
;; only Procedure

(: cgen-close/list (-> Nonnegative-Integer ;; nb-state
                       Nonnegative-Integer ;; nb-in
                       TargetListFunction  ;; open function
                       TargetListFunction  ;; closed function
                       ))

;; Sample non-var references in a var.  It's a lot simpler to make
;; most of the interfaces work with vars only.  Low level compiler can
;; easily remove unnecessary var copy operations.
(: as-var! (-> cgen Ref var))
(define (as-var! s ref)
  (if (var? ref) ref
      ;; FIXME: dims?  Or assume ref is scalar?
      (bind1! s 'v "copy" ref)))

(define (cgen-close/list nb-state nb-in update)
  (lambda (s inref)
    ;; (log/pp "instance "  update)
    (let*
        ((in : (Listof var)
             (for/list ((r inref)) (as-var! s r)))
        ;; The core principle of the dsp stream language is that a
        ;; stateful stream processor instance corresponds to the
        ;; _application_ of the function that represents it, not the
        ;; function abstraction itself.  This means that new state
        ;; variables corresponding to this instance need to be added
        ;; to the top level C function's state when processor
        ;; representing function is _applied_.  And one state slot
        ;; needs to be allocated for each point in a (nested) spatial
        ;; iteration.  Note that state variables contain dims (coords
        ;; + sizes), not just coords.
         (state : (Listof var)
                (for/list ((i (in-range nb-state)))
                          (make-state! s (loop-dims s))))
         ;; (_ (comment! s state))
         ;; Buffer the state, see footnote (1).
         (_ (code! s (comment "feedback state snapshot")))
         (state-in : (Listof var)
                   (for/list ((si state))
                             (bind1! s 'v "copy" si)))
         (state-in-and-in : (Listof var)
                          (append state-in in))                
         )
            
            
      ;;(log/pp "  state:     " state)
      ;;(log/pp "  state-in:  " state-in)
      ;;(log/pp "  in:        " in)
      
      (code! s (comment "feedback body"))
           
      (let*-values
          ((([retvals : (Listof var)])
            (update s state-in-and-in))
           (([state-out : (Listof var)]
             [out       : (Listof var)])
            (split-at retvals nb-state)))
        ;;(log/pp "  state-out: " state-out)
        ;;(log/pp "  out:       " out)
        (code! s (comment "feedback state update"))
        (for ((dst state) (src state-out))
             (code! s (assign dst '() src)))
        out)
      )))


(: in-array! (-> cgen Nonnegative-Integer * var))
(define (in-array! s . dims)
  (make-array-var! s (for/list ((d : Nonnegative-Integer dims)) (dim #f d)) 'i))

(: in-scalar! (-> cgen var))
(define (in-scalar! s)
  (make-var! s 'i))

(: cgen-meta! (-> cgen var Any Void))
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


