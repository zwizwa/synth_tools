#lang typed/racket/base
(require
 "sig.rkt"
 typed/racket/unit 
 racket/match
 racket/list
 racket/pretty
 )

;; TODO:
;; - Go over all Any, Symbol types

;; (struct const (value)                 #:transparent)
;; (struct function (state in code out)  #:transparent)


(struct reg ([type : Symbol]
             [dims : Any]
             [tag  : Symbol]
             [nb   : Integer])
        #:transparent)

(struct dim ([reg  : reg]
             [size : Integer])
        #:transparent)

;; FIXME: RHS can be reg or literal or array ref.  Bundle those into
;; the value type.
(struct bind ([reg : reg] [op : Symbol] [args : (Listof reg)]) #:transparent)
(struct array ([reg : reg]) #:transparent)
(struct assign ([dst : reg] [src : reg]) #:transparent)
(struct array-assign ([reg : reg] [coords : (Listof reg)] [src : reg]) #:transparent)
(struct array-ref ([reg : reg] [coords : (Listof reg)]) #:transparent)
(struct loop ([iter : reg] [stop : Any] [code : (Listof Code)]) #:transparent)
(struct comment ([msg : String]) #:transparent)


(define-type Code (U bind array assign array-assign loop comment))

(struct slice ([parent : reg]
               [coords : (Listof reg)])
        #:transparent)



(struct cgen
        ([next-reg : (Mutable-HashTable Symbol Integer)]
         [code     : (Listof Code)]
         [state    : (Listof reg)]
         [stack    : (Listof (Listof Code))]
         [dims     : (Listof dim)]
         [slice    : (Mutable-HashTable reg slice)]
         [meta : (Listof (Pair reg Any))]
         ) #:mutable #:transparent)

(define (init-cgen)
  (cgen
   (make-hash '((i . 0) (o . 0) (s . 0) (l . 0))) ;; next-reg
   '() ;; code
   '() ;; state
   '() ;; stack
   '() ;; dims
   (make-hash) ;; slice
   '() ;; meta
   ))


(: make-generic-reg!
   (-> cgen Symbol (Listof dim) Symbol
       reg))
(define (make-generic-reg! s type dims tag)
  (let* ((h (cgen-next-reg s))
         (nb (hash-ref h tag)))
    (hash-set! h tag (add1 nb))
    (reg type dims tag nb)))
