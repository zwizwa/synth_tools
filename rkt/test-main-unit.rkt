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
    (let*-values
        (((n_t) 64) ;; Number of time instances to compute.
         ((n_voices) (sizeof osc_inc))
         ((osc_inc_delta) (deltas osc_inc n_t))
         ;; Oscillator
         ((osc) (close 1 (lambda (s inc) (values (frac (+ s inc)) s))))
         ((_ mix_out)
          ;; Outer loop is a time loop producing n_t time samples.
          (time n_t
                ;; Initialize the loop state = interpolated oscillator
                ;; increments.
                (lambda ()
                  (loop n_voices
                        (lambda (i) (ref osc_inc i))))
                
                ;; Inner loop is the voice loop, calculating each
                ;; oscillator and summing it together.  The loop state
                ;; is the mix value, which is implicitly initialized
                ;; to 0 due to lack of initializer form.
                (lambda (t interp_inc)
                  (let*-values
                      (((out interp_inc_next)
                        (loop n_voices
                              (lambda (voice_nb mix)
                                (let* ((inc (ref interp_inc voice_nb)))
                                  (values
                                   ;; State update = accumulate voice output.
                                   (+ mix (osc inc))
                                   ;; Additional output = increment of
                                   ;; voice interpolator used as outer
                                   ;; loop state.
                                   (+ inc (ref osc_inc_delta voice_nb))))))))
                    (values interp_inc_next
                            out)))
                )))
      mix_out))


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
       ;; The canonical stateful processor to show tracking of state.
       (close 1 (lambda (s i) (values (+ s i) s))))

      ((procproc)
       ;; Double application of integrator to show tracking of
       ;; separate state per application.
       (lambda (i)
         (let* ((update (lambda (s i) (values (+ s i) s)))
                (proc (close 1 update)))
           ;; Invoke it twice to check that each instantiation has its own
           ;; state registers.
           (proc (proc i)))))

      ((sumramp)
       ;; Use spatial loop to sum the output of 3 stateful generators.
       (lambda (in)
         ;; Create ramp generators
         (let ((ramp (close 1 (lambda (s) (values (+ s in) s)))))
           (loop 3  ;; 0,1,2
                 (lambda (i s)
                   ;; Sum the output of a couple of ramp generators.
                   (+ s (ramp)))))))

      ((matrix)
       ;; Use spatial loop to construct a matrix.
       (lambda ()
         (loop 3 (lambda (i) ;; 0,1,2
         (loop 4 (lambda (j) ;; 0,1,3,4
           (values (* i j))))))))

      ((timeloop)
       ;; Simple time loop to illustrate semantics: state inside the
       ;; timeloop is updated.
       (lambda ()
         (let ((ramp (close 1 (lambda (s) (values (+ s 1) s)))))
           (time
            64
            (lambda (t)
              (ramp))))))

      ((timeloopparam)
       ;; Time loop with interpolated parameters, for block-based
       ;; 2-rate inputs.  The idea is to later create a macro or a
       ;; higher order function for a time loop with interpolated
       ;; parameters and have it expand into something like this.
       (lambda (in param)
         (let* ((_ (meta! in    '((name . "In")    (unit . ms) (min . 1)  (max . 1000))))
                (_ (meta! param '((name . "Param") (unit . hz) (min . 20) (max . 20000))))
                (dparam (D param)) ;; Delayed parameter
                (incparam (/ (- param dparam) (sizeof in))))
           (time
            (sizeof in)
            (lambda (t iparam)
              (let* ((x (integrate (ref in t)))
                     (y (integrate x)))
                (values (+ iparam incparam) x y)
                ))))))

      ((loopinit)
       ;; Spatial loop with separate state initializer.
       (lambda ()
         (loop 4
               ;; Initialze the 2 state variables.
               (lambda ()
                 (values 123 456))
               ;; Update the 2 state variables (identity nop here).
               (lambda (i s1 s2)
                 (values s1 s2)))))

      ((loopstateinit)
       (lambda (osc_inc)
         (let*-values
             (((_ out)
               (loop 10
                     (lambda ()
                       ;; Test instantiates osc_init with 64 a element
                       ;; vector.  Note that in the generated code the
                       ;; initializer loop should appear before the
                       ;; loop 0..9 starts.
                       ;;
                       ;; This explicit copy seems to work
                       ;(loop (sizeof osc_inc) (lambda (i) (ref osc_inc i)))

                       ;; But this does not: it puts the init inside
                       ;; the loop.

                       osc_inc
                       )
                     (lambda (i loopstate)
                       ;;(loop (sizeof loopstate)
                       ;;      (lambda (i) (+ 1 (ref loopstate i))))
                       (values loopstate 123)
                       )
                     )))
           out)))

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
                      ;; Loop state init
                      (lambda ()
                        ;; FIXME: First fix the loop form, then the array form.
                        ;;osc_inc
                        (loop n_voices
                              (lambda (i) (ref osc_inc i)))
                        )
                      ;; FIXME: Iterate over all voices, accumulate output
                      ;; and increment the inc state.
                      (lambda (t interp_inc)
                        (let*-values
                            (((out interp_inc_next)
                              (loop n_voices
                                    (lambda (i mix)
                                      (let ((interp_inc_i (ref interp_inc i))
                                            (deltas_i     (ref deltas i)))
                                        (values
                                         (+ mix
                                            (osc interp_inc_i))
                                         (+ interp_inc_i deltas_i)))))))
                          (values interp_inc_next
                                  out)))
                      )))
            out)))
      ((synth) synth)
      ))
  )
(provide main@)
