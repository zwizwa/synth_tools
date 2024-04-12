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

  
  ;; (define (main a b c) (+ a (+ b c)))
  (define (main1 i)
    (let* ((update (lambda (s i) (values (+ s i) s)))
           (proc (close 1 update)))
      ;; Invoke it twice to check that each instantiation has its own
      ;; state registers.
      (proc (proc i))))

  (define main0 (close 1 (lambda (s i) (values (+ s i) s))))

  ;; TODO: Add a test to see if lambdas can create illegal forms,
  ;; i.e. constructs that cannot be compiled.

  (define (main2)
    (loop
     3 (lambda (i s)
         (+ s i))))

  (define (main3 in)
    ;; Create ramp generators
    (let ((ramp (close 1 (lambda (s) (values (+ s in) s)))))
      (loop 3
            (lambda (i s)
              ;; Sum the output of a couple of ramp generators.
              (+ s (ramp))))))

  ;; Abstract combinator in terms of array operations.
  (define (map/sum osc inc)
    (loop (sizeof inc)
          (lambda (i acc)
            (+ acc (osc (ref inc i))))))

  (define (main4 osc_inc)
    (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
      (map/sum osc osc_inc)))

  (define (main dummy)
    (loop 3 (lambda (i) (loop 4 (lambda (j) (values (* i j)))))))

  ;; TODO features:
  ;; - sizeof, so that mix-proc doesn't need size
  ;; - rename for input and output field to make structs usable
    
)
(provide main@)
