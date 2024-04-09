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
      (proc i)))

)
(provide main@)
