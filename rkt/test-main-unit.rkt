#lang s-exp "dsp.rkt"
;; dsp.rkt redefines: lambda #%app define
;; and exposes basics: require provide
(require
 racket/unit
 "sig.rkt"
)

(define-unit main@

  (import field^ field-lib^ loop^ stream^ float^ meta^)
  (export main^)


  ;; Abstract combinator in terms of array operations.
  (define (map/sum osc inc)
    (loop (sizeof inc)
          (lambda (i acc)
            (+ acc (osc (ref inc i))))))

  ;; FIXME put synth in a separate module.
  
  ;; (define (synth osc_inc)
  ;;   (let* (;;(_ (meta! in    '((name . "In")    (unit . ms) (min . 1)  (max . 1000))))
  ;;          ;;(_ (meta! param '((name . "Param") (unit . hz) (min . 20) (max . 20000))))
  ;;          ;;(dparam (D param))
  ;;          ;;(incparam (/ (- param dparam) (sizeof in)))
  ;;          )
  ;;     (time
  ;;      1024 ;; (sizeof <something>)
  ;;      (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
  ;;        (map/sum osc osc_inc)))))

  (define (deltas osc_inc n)
    (loop (sizeof osc_inc)
          (lambda (i)
            (let* ((invn (/ 1 n))
                   (oi (ref osc_inc i))
                   (Doi (D oi)))
              (* (- oi Doi) invn)))))
  
  (define (synth osc_inc)
    (let* ((n 1024)
           (ds (deltas osc_inc n)))
      (time n
            ;; FIXME: use deltas to update
            (lambda (t)
              (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
                (map/sum osc osc_inc))))))

  ;;(define (synth osc_inc)
  ;;  (let* ((osc (close 1 (lambda (s inc) (values (frac (+ s inc)) s)))))
  ;;    (map/sum osc osc_inc)))
      
  (define integrate
    (close 1 (lambda (s i) (let ((sn (+ s i))) (values sn sn)))))
  (define D
    (close 1 (lambda (s i) (values i s))))
  
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

      ((timeloop)
       ;; Time loop with interpolated parameters, for block-based
       ;; 2-rate inputs.
       (lambda (in param)
         (let* ((_ (meta! in    '((name . "In")    (unit . ms) (min . 1)  (max . 1000))))
                (_ (meta! param '((name . "Param") (unit . hz) (min . 20) (max . 20000))))
                (dparam (D param))
                (incparam (/ (- param dparam) (sizeof in))))
           (time
            (sizeof in)
            (lambda (t iparam)
              (let* ((x (integrate (ref in t)))
                     (y (integrate x)))
                (values (+ iparam incparam) x y)
                ))))))

      ((loopinit)
       (lambda ()
         (loop 4
               (lambda ()
                 (values 123 456))
               (lambda (i s1 s2)
                 (values s1 s2)))))

      ((loopstateinit)
       (lambda (osc_inc)
         (loop 10
               (lambda ()
                 ;; FIXME: First fix the loop form, then the array form.
                 ;osc_inc
                 (loop (sizeof osc_inc)
                       (lambda (i) (ref osc_inc i)))
                 )
               (lambda (i loopstate)
                 ;;(loop (sizeof loopstate)
                 ;;      (lambda (i) (+ 1 (ref loopstate i))))
                 loopstate
                 )
               )))

      ((interpol)
       ;; 1. control rate state machine ('D' operator used outside of time loop)
       ;; 2. signal rate interpolation (loop state in time loop)
       ;; 3. signal rate state machine (use of osc)
        (lambda (osc_inc)
          (let*-values
              (((osc) (close 1 (lambda (s i) (values (+ s i) s))))


               ((n_t) 1024)
               ((invn_t) (/ 1 n_t))
               ((n_voices) (sizeof osc_inc))
               ;; Interpolated delta.  FIXME: Better to compute or to
               ;; create intermediates?
               ((deltas) 
                (loop n_voices
                      (lambda (i)
                        (let* ((oi (ref osc_inc i))
                               (Doi (D oi)))
                          (* (- oi Doi) invn_t)))))
               ((_ out)
                (time n_t
                      (lambda ()
                        ;; FIXME: First fix the loop form, then the array form.
                        ;;osc_inc
                        (loop (sizeof osc_inc)
                              (lambda (i) (ref osc_inc i)))
                        )
                      ;; FIXME: Iterate over all voices, accumulate output
                      ;; and increment the inc state.
                      (lambda (t interp_inc)
                        (let*-values
                            (((out interp_inc_next)
                              (loop n_voices
                                    (lambda (i mix)
                                      (values
                                       (+ mix
                                          (osc interp_inc))
                                       (+ interp_inc (ref deltas i)))))))
                          (values interp_inc_next
                                  out)))
                      )))
            out)))
      ))
  )
(provide main@)
