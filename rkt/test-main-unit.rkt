#lang s-exp "dsp.rkt"
;; dsp.rkt redefines: lambda #%app define
;; and exposes basics: require provide
(require
 racket/unit
 "field-sig.rkt"
 "field-lib-sig.rkt"
 "close-sig.rkt"
 "main-sig.rkt"
)

(define-unit main@

  (import field^ field-lib^ close^)
  (export main^)

  
  ;; (define (main a b c) (+ a (+ b c)))
  (define (main i)
    (let* ((update (lambda (s i) (values (+ s i) s)))
           (proc (close 1 update)))
      ;; Invoke it twice to check that each instantiation has its own
      ;; state registers.
      (proc (proc i))))

  (define main0 (close 1 (lambda (s i) (values (+ s i) s))))

)
(provide main@)
