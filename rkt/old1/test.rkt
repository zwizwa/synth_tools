#lang racket/base

(require
 (file "test_prog.rkt") ;; This binds dsp-module
 racket/pretty
 )

;; Evaluator
(let*
    ((inc! (lambda (s) (set-box! s (add1 (unbox s)))))
     (add  (lambda (s a b) (inc! s) (+ a b)))
     (sub  (lambda (s a b) (inc! s) (- a b)))
     (prog (dsp-module add sub))
     ;; Just an evaluation counter
     (state (box 0)))
    
  (display "eval:\n")
  ;; Create primitives for eval semantics
  (pretty-print
   `((result ,(prog state 1 2))
     (end-state , (unbox state)))))

;; Compiler
(let*
    ((bind (lambda (s op . args)
             (let* ((s0 (unbox s))
                    (n (length s0))
                    (s1 `((,n (,op . ,args)) . ,s0)))
               (set-box! s s1)
               `(ref ,n))))
     (add (lambda (s a b) (bind s '+ a b)))
     (sub (lambda (s a b) (bind s '- a b)))
     (prog (dsp-module add sub))
     (state (box '())))
    
  (display "compile:\n")
  ;; Create primitives for eval semantics
  (pretty-print
   `((result ,(prog state `(const 1) `(const 2)))
     (end-state , (unbox state)))))

