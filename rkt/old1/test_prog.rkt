#lang s-exp (file "dsp.rkt")

;; All exported functions are parameterized by this list of primitive
;; operations.
(primitives + -)


;(require (file "lib.rkt"))

;; The second expression is the expression that implements the
;; function exposed by the module.
(define dsp-module
  (lambda (s i)
    (- (+ s i) i)
    ))

;; FIXME: Should this allow import of other modules?  That will be
;; tricky because those will also need to be parameterized by the
;; primitives.  Let's not worry about this kind of composition and
;; always inject all symbols.
