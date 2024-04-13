#lang s-exp "dsp.rkt"
;; dsp.rkt redefines: lambda #%app define
;; and exposes basics: require provide
(require
 racket/unit
 "field-sig.rkt"
 "field-lib-sig.rkt"
 "stream-sig.rkt"
 "main-sig.rkt"
)

(define-unit main@

  (import field^ field-lib^ stream^)
  (export main^)


  ;; Abstract combinator in terms of array operations.
  (define (map/sum osc inc)
    (loop (sizeof inc)
          (lambda (i acc)
            (+ acc (osc (ref inc i))))))

  ;; FIXME put this in a separate module.
  (define (synth osc_inc)
    (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
      (map/sum osc osc_inc)))

  ;; A main^ unit only defines one function, so we use that to
  ;; dispatch on a symbol to return one of the test cases.  The inputs
  ;; need to be generated in test-cgen.rtk for cgen tests.
  (define (main example)
    (case example

      ((integrator)
       (close 1 (lambda (s i) (values (+ s i) s))))

      ((procproc)
       (lambda (i)
         (let* ((update (lambda (s i) (values (+ s i) s)))
                (proc (close 1 update)))
           ;; Invoke it twice to check that each instantiation has its own
           ;; state registers.
           (proc (proc i)))))

      ((sumramp)
       (lambda (in)
         ;; Create ramp generators
         (let ((ramp (close 1 (lambda (s) (values (+ s in) s)))))
           (loop 3
                 (lambda (i s)
                   ;; Sum the output of a couple of ramp generators.
                   (+ s (ramp)))))))

      ((matrix)
       (lambda ()
         (loop 3 (lambda (i)
         (loop 4 (lambda (j)
           (values (* i j))))))))

      ((synth)  synth)
      (else #f)))

  ;; TODO features:
  ;; - sizeof, so that mix-proc doesn't need size
  ;; - rename for input and output field to make structs usable
    
)
(provide main@)
