#lang racket/base
(require ffi/unsafe
         ffi/unsafe/define)
 
;; https://docs.racket-lang.org/foreign/index.html
;; https://docs.racket-lang.org/foreign/Defining_Bindings.html
(define-ffi-definer define-test-id (ffi-lib "../linux/test_cgen.dynamic.host"))
(define-test-id test (_fun -> _void))
(test)

