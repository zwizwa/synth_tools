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

  (define (main osc_inc)
    (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
      (loop 64
            (lambda (i mix)
              (+ mix (osc (ref osc_inc i)))))))
    
)
(provide main@)
