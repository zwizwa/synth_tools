#lang s-exp "racket-base.rkt"
(require
 (prefix-in racket: racket/base)
 racket/match
 racket/unit)
(define-signature field^ (+ - * /))
(define-signature basefield^ (^+ ^- ^* ^/))

;; To express one field in terms of another field we use a basefield^
;; unit that has all field names prefixed.

;; Interface rename
(define-unit field-to-basefield@
  (import field^)
  (export basefield^)
  (define ^+ +)
  (define ^- -)
  (define ^* *)
  (define ^/ /))


;; Complex number type
(struct C (r i) #:transparent)

;; Complex field in terms of base field.
(define-unit complex@
  (import basefield^)
  (export field^)

  (define/match (+ a b)
    (((C ar ai) (C br bi))
     (C (^+ ar br) (^+ ai bi))))

  (define/match (- a b)
    (((C ar ai) (C br bi))
     (C (^- ar br) (^- ai bi))))

  (define/match (* a b)
    (((C ar ai) (C br bi))
     (C (^- (^* ar br) (^* ai bi))
        (^+ (^* ar bi) (^* ai br)))))

  (define / #f)
  
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
(define-compound-unit/infer racket-basefield@
    (import)
    (export basefield^)
    (link racket-field@
          field-to-basefield@))
(define-compound-unit/infer racket-complex@
    (import)
    (export field^)
    (link
     complex@
     racket-basefield@))

;; Bind identifiers in this module
(define-values/invoke-unit/infer racket-complex@)

;; And run some tests.
(+ (C 1 2) (C 3 4))
(* (C 1 2) (C 3 4))
   

