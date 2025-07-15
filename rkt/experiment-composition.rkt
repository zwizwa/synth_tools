#lang typed/racket/base
(require
 racket/match
 )


;; Key observations:
;;
;; * The point of the DSP language (besides the base language, which
;;   is almost trivial) is to formalize the combinators.
;;
;; * In a composition, there are 3 things that are combined:
;;
;;   * Signal input/output behavior (arrow), or parameterized signal
;;     view (applicative).  This is the programmer interface.
;;
;;   * Causal state and init propagation.  This is seen only by the
;;     compiler
;;
;;   * Parameter hierarchy. This is user interface.
;;
;;   It seems necessary to treat these separately.
;;
;; * Embedding this in an existing type system seems to be an
;;   impediment.  I think I first want to write the type syntax
;;   manipulation in a direct style and once it works, then find a way
;;   to transform it into something that embeds in typed scheme or
;;   Rust or Haskell to check consistency.  Scheme seems best for now.


;; The "view" taken in representation is to focus on signals (or
;; grouping of signals) as the things that are manipulated, and keep
;; track of the state and the parameters in the background.

;; At some point there will need to be "insertions" that transform
;; parameters into signals.  This can contain control rate unit
;; conversion and signal rate interpolation.





;; Do the state product first, then the rest.

(struct Flow   ())
(struct State  ())
(struct Param  ())


;; The Signal datatype always lives in a context (the current compilation state).

(struct Signal
  ([flow  : Flow]   ;; dataflow node
   [state : State]  ;; hidden causal signal state
   [param : Param]  ;; hidden signal parameters
   ))

(define (compose A B)
  (match* (A B)
    (([Signal flow1 state1 param1]
      [Signal flow2 state2 param2])
     (Signal flow1 state1 param1))))
      
    
compose

(define s1 (Signal (Flow) (State) (Param)))

(compose s1 s1)
