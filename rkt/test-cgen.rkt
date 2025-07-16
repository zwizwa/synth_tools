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
 ;; "cgen.rkt"
 "untyped-cgen.rkt"
 "typed-cgen.rkt"
 )

;; Instantiate and introduces identifies into this module's namespace.
(define-values/invoke-unit/infer cgen@)
(define-values/invoke-unit/infer field-lib@)
(define-values/invoke-unit/infer main@)


;; Run the compiler for a specific example defined as a case in main.
(define (compile-example c-code-port example make-ins)
  (let*
      ;; The .info file contains compiler state
      ((info-port (open-output-file (format "~a.info" example)  #:exists 'replace))

       ;; Create compiler initial state.
       (s (init-cgen))

       ;; Select the hoas function to compile
       (f (main s example))

       ;; Compile the hoas function to intermediate representation.
       (compiled-f (apply compile s f (make-ins s))))

    ;; Pretty-print the intermediate representation to the .info file.
    ;; The pretty printer doesn't use a lexical parameter so set the
    ;; dynamic parameter.
    (parameterize ((current-output-port info-port))
      (pp-function compiled-f))

    ;;(display "slices:\n")
    ;;(pp (hash-map (cgen-slice s) cons))
    ;;(pp (cgen-slice s))

    ;; Pretty-print the intermediate representation as C code
    (fwrite-c-code s compiled-f example c-code-port)
    
    ;; (log/pp "meta:\n" (cgen-meta s))
    (close-output-port info-port)
    ))


;; Input argument constructors.
(define (G s) '())  ;; nothing. this is just a generator
(define (S s) (list (in-scalar! s))) ;; single scalar
(define (V s) (list (in-array! s 64))) ;; single vector
(define (VS s) (list (in-array! s 64) (in-scalar! s))) ;; vector, scalar


(let ((port (open-output-file "../generic/cgen_test_out.h"  #:exists 'replace))
      )
  (for
   ((example-spec
     `(
       (integrator    ,S)
       (procproc      ,S)
       (sumramp       ,S)
       (matrix        ,G)
       (timeloop      ,G)
       (timeloopparam ,VS)
       (loopinit      ,G)
       (loopstateinit ,V)
       (interpol      ,V)
       ;;(synth  ,V)
       )))

    (let ((name (car example-spec)))
      (pp name)
      (display (format "/////////////////////////// ~s\n" name) port)
      (apply compile-example port example-spec)
      (display "\n" port)
      )))


(when #t
  ;; FIXME: For now the synth engine is defined together with all the
  ;; test programs, but it goes into a separate file to be included in
  ;; synth.c
  (pp 'synth)
  (compile-example
   (open-output-file "../generic/cgen_synth_out.h" #:exists 'replace)
   'synth
   V))


