#lang racket/base
(require ffi/unsafe
         ffi/unsafe/define)
 
(define-ffi-definer define-curses (ffi-lib "../linux/test_cgen.dynamic.host"))

;; https://docs.racket-lang.org/foreign/index.html

;; Seems to do something.  Next step is to build a .so

;; tom@tp:/i/exo/synth_tools/rkt$ racket test-ffi.rkt 
;; ffi-lib: could not load foreign library
;;   path: ../linux/synth_tools.dynamic.host.so
;;   system error: ../linux/synth_tools.dynamic.host.so: undefined symbol: gensym
;;   context...:
;;    /nix/store/l027bv0l4snn4yfzbx54kf2nd9b3hj2i-racket-8.10/share/racket/collects/ffi/unsafe.rkt:131:0: get-ffi-lib
;;    body of "/i/exo/synth_tools/rkt/test-ffi.rkt"
