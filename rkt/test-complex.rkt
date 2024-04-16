#lang s-exp "racket-base.rkt"
(require
 (prefix-in racket: racket/base)
 racket/match
 racket/unit)
(define-signature field^ (+ - * /))
(define-signature basefield^ (base:+ base:- base:* base:/))

;; Complex numer type
(struct complex (r i) #:transparent)

;; Interface rename
(define-unit field-to-basefield@
  (import field^)
  (export basefield^)
  (define base:+ +)
  (define base:- -)
  (define base:* *)
  (define base:/ /))

;; Complex field in terms of base field.
(define-unit complex@
  (import basefield^)
  (export field^)
  (define/match (+ a b)
    (((complex ar ai) (complex br bi))
     (complex (base:+ ar br) (base:+ ai bi))))
  (define - #f)
  (define * #f)
  (define / #f)
  )

;; Field in terms of Racket number operations.
(define-unit racket-field@
  (import)
  (export field^)
  (define + racket:+)
  (define - racket:-)
  (define * racket:*)
  (define / racket:/)
  )

;; Instantiate complex field in terms of racket number ops.

(define-compound-unit/infer racket-basefield@
    (import)
    (export basefield^)
    (link racket-field@
          field-to-basefield@))
          

(define-compound-unit/infer racket-complex@
    (import)
    (export field^)
    (link racket-basefield@
          complex@))

(define-values/invoke-unit/infer racket-complex@)

(+ (complex 1 2) (complex 3 4))
   
