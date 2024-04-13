#lang s-exp "racket-base.rkt"
(require
 racket/pretty
 racket/unit

 ;; Signatures
 "sig.rkt"

 ;; Generic code
 "field-lib-unit.rkt"
 "test-main-unit.rkt"
 
 ;; Substrate implementation
 "cgen.rkt"
 )

;; Instantiate and introduces identifies into this module's namespace.
(define-values/invoke-unit/infer cgen@)
(define-values/invoke-unit/infer field-lib@)
(define-values/invoke-unit/infer main@)


;; Run the compiler for a specific example defined as a case in main.
(define (compile-example port example make-ins)
  (let*
      ((s (init-cgen))
       (f (main s example)) ;; Select the function to compile
       (compiled-f (apply compile s f (make-ins s))))
    (pp-function compiled-f)
    ;;(display "slices:\n")
    ;;(pp (hash-map (cgen-slice s) cons))
    ;;(pp (cgen-slice s))
    ;; (fwrite-c-code (current-output-port)  f)
    (fwrite-c-code s compiled-f example port)))


;; Input argument constructors.
(define (G s) '())  ;; nothing. this is just a generator
(define (S s) (list (in-scalar! s))) ;; single scalar
(define (V s) (list (in-array! s 64))) ;; single vector


(let ((port (open-output-file "../generic/cgen_test_out.h"  #:exists 'replace))
      )
  (for
   ((example-spec
     `((matrix     ,G)
       (integrator ,S)
       (procproc   ,S)
       (sumramp    ,S)
       ;; (synth  ,V)
       (timeloop   ,V)
       )))
   (pp (car example-spec))
   (apply compile-example port example-spec)))

;; FIXME: For now the synth engine is defined together with all the
;; test programs, but it goes into a separate file to be included in
;; synth.c
(pp 'synth)
(compile-example
 (open-output-file "../generic/cgen_synth_out.h" #:exists 'replace)
 'synth
 V)


