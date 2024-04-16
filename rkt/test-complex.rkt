#lang s-exp "racket-base.rkt"
(require
 (prefix-in racket: racket/base)
 racket/match
 racket/unit)
(define-signature field^ (+ - * /))
(define-signature complex-field^ (c:+ c:- c:* c:/))

;; Express one field in terms of another field, and provide a rename
;; for the interface to use the compound field as a field^ input of
;; other units.

;; Interface rename
(define-unit complex-field-to-field@
  (import complex-field^)
  (export field^)
  (define + c:+)
  (define - c:-)
  (define * c:*)
  (define / c:/))


;; Complex number type
(struct C (r i) #:transparent)

;; Complex field in terms of base field.
(define-unit complex@
  (import field^)
  (export complex-field^)

  (define/match (c:+ a b)
    (((C ar ai) (C br bi))
     (C (+ ar br) (+ ai bi))))

  (define/match (c:- a b)
    (((C ar ai) (C br bi))
     (C (- ar br) (- ai bi))))

  (define/match (c:* a b)
    (((C ar ai) (C br bi))
     (C (- (* ar br) (* ai bi))
        (+ (* ar bi) (* ai br)))))

  (define c:/ #f)
  
  )

;; To test, defined field^ interface in terms of Racket number
;; operations.  Note that "racket-base.rkt" doesn't export + - / *
(define-unit racket-field@
  (import)
  (export field^)
  (define + racket:+)
  (define - racket:-)
  (define * racket:*)
  (define / racket:/)
  )

;; Instantiate complex field in terms of racket number ops.
(define-compound-unit/infer racket-complex@
    (import)
    (export complex-field^)
    (link
     complex@
     racket-field@))
;; And rename it to field^ interface
(define-compound-unit/infer racket-test@
    (import)
    (export field^)
    (link racket-complex@
          complex-field-to-field@))

;; Bind identifiers in this module
(define-values/invoke-unit/infer racket-test@)

;; And run some tests.
(+ (C 1 2) (C 3 4))
(* (C 1 2) (C 3 4))
   

