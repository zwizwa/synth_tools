#lang racket/base
(require
 racket/unit)
(provide
 (all-defined-out))
(define-signature float^ (frac))
(define-signature field^ (+ - * /))
(define-signature field-lib^ (add3))
(define-signature main^ (main))
(define-signature loop^
  (loop   ;; run a processor, collect state and output
   ref    ;; array reference
   sizeof ;; array dimensions (multiple values)

   time   ;; like loop, but as an outer time loop
))
(define-signature stream^
  (close  ;; close over time, feeding back via delay
))
(define-signature meta^
  (meta!  ;; attach extra metadata to a thing
))

;; If should not use conditional execution, because that will make
;; state machines misbehave.  Look at how rai does it.

