#lang racket/signature
close    ;; close over time, feeding back via delay
iterate  ;; iterate over space


;; FIXME: Maybe better to split off iterate because I do think it is
;; meaningful to have things like array processors that are guaranteed
;; not to be recursive systems.